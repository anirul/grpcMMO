#include "GrpcSessionClient.hpp"

#include <utility>

#include "grpcmmo/shared/Time.hpp"

namespace grpcmmo::client
{
namespace
{
std::string ToAddress(const grpcmmo::auth::v1::RealmEndpoint& endpoint)
{
    return endpoint.host() + ":" + std::to_string(endpoint.port());
}
}

GrpcSessionClient::GrpcSessionClient() = default;

GrpcSessionClient::~GrpcSessionClient()
{
    Shutdown();
}

bool GrpcSessionClient::Connect(const ClientConnectionConfig& config,
                                std::string* error_message)
{
    const grpc::Status previous_status = Shutdown();
    if (!previous_status.ok())
    {
        if (error_message != nullptr)
        {
            *error_message = "previous stream shutdown failed: " +
                             previous_status.error_message();
        }
        return false;
    }

    const auto auth_channel =
        grpc::CreateChannel(config.auth_server_address,
                            grpc::InsecureChannelCredentials());
    auto auth_stub = grpcmmo::auth::v1::AuthService::NewStub(auth_channel);

    grpc::ClientContext login_context;
    grpcmmo::auth::v1::LoginRequest login_request;
    login_request.set_login_name(config.login_name);
    login_request.set_password(config.password);
    login_request.set_client_build("grpcmmo-frame-client-dev");
    grpcmmo::auth::v1::LoginResponse login_response;
    const grpc::Status login_status =
        auth_stub->Login(&login_context, login_request, &login_response);
    if (!login_status.ok())
    {
        if (error_message != nullptr)
        {
            *error_message = "login failed: " + login_status.error_message();
        }
        return false;
    }

    grpcmmo::auth::v1::CharacterSummary selected_character;
    if (!FetchOrCreateCharacter(config, login_response, &selected_character, error_message))
    {
        return false;
    }

    grpc::ClientContext grant_context;
    grpcmmo::auth::v1::CreateSessionGrantRequest grant_request;
    grant_request.set_account_access_token(login_response.account_access_token());
    grant_request.set_realm_id(config.realm_id);
    grant_request.set_character_id(selected_character.character_id());
    grpcmmo::auth::v1::CreateSessionGrantResponse grant_response;
    const grpc::Status grant_status =
        auth_stub->CreateSessionGrant(&grant_context, grant_request, &grant_response);
    if (!grant_status.ok())
    {
        if (error_message != nullptr)
        {
            *error_message = "create session grant failed: " +
                             grant_status.error_message();
        }
        return false;
    }

    if (!OpenSessionStream(config, selected_character, grant_response, error_message))
    {
        return false;
    }

    StartReader();
    return true;
}

bool GrpcSessionClient::SendMove(const MoveCommand& move_command)
{
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (!session_stream_)
    {
        return false;
    }

    grpcmmo::session::v1::ClientMessage input_message;
    auto* input = input_message.mutable_input_frame();
    const auto sequence = next_input_sequence_.fetch_add(1, std::memory_order_relaxed);
    input->set_command_id("move-" + std::to_string(sequence));
    input->set_client_time_ms(grpcmmo::shared::NowMs());
    input->set_input_sequence(sequence);
    auto* move = input->mutable_move();
    move->mutable_world_displacement_m()->set_x(
        static_cast<double>(move_command.world_displacement_m.x));
    move->mutable_world_displacement_m()->set_y(
        static_cast<double>(move_command.world_displacement_m.y));
    move->mutable_world_displacement_m()->set_z(
        static_cast<double>(move_command.world_displacement_m.z));
    move->set_sprint(move_command.sprint);
    if (move_command.has_facing_orientation)
    {
        auto* facing_orientation = move->mutable_facing_orientation();
        facing_orientation->set_x(static_cast<double>(move_command.facing_orientation.x));
        facing_orientation->set_y(static_cast<double>(move_command.facing_orientation.y));
        facing_orientation->set_z(static_cast<double>(move_command.facing_orientation.z));
        facing_orientation->set_w(static_cast<double>(move_command.facing_orientation.w));
    }
    return session_stream_->Write(input_message);
}

bool GrpcSessionClient::SendPing()
{
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (!session_stream_)
    {
        return false;
    }

    grpcmmo::session::v1::ClientMessage ping_message;
    ping_message.mutable_ping()->set_nonce("frame-client-ping");
    return session_stream_->Write(ping_message);
}

void GrpcSessionClient::PollMessages(
    const std::function<void(const grpcmmo::session::v1::ServerMessage&)>& on_message)
{
    std::deque<grpcmmo::session::v1::ServerMessage> pending;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pending.swap(pending_messages_);
    }

    for (const auto& message : pending)
    {
        on_message(message);
    }
}

grpc::Status GrpcSessionClient::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (session_context_)
        {
            session_context_->TryCancel();
        }
    }

    if (reader_thread_.joinable())
    {
        if (reader_thread_.get_id() == std::this_thread::get_id())
        {
            reader_thread_.detach();
        }
        else
        {
            reader_thread_.join();
        }
    }

    grpc::Status status = grpc::Status::OK;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (session_stream_)
        {
            session_stream_->WritesDone();
            status = session_stream_->Finish();
            session_stream_.reset();
        }
        session_context_.reset();
        session_stub_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pending_messages_.clear();
    }

    next_input_sequence_.store(1, std::memory_order_relaxed);
    open_.store(false, std::memory_order_relaxed);
    return status;
}

bool GrpcSessionClient::IsOpen() const
{
    return open_.load(std::memory_order_relaxed);
}

bool GrpcSessionClient::FetchOrCreateCharacter(
    const ClientConnectionConfig& config,
    const grpcmmo::auth::v1::LoginResponse& login_response,
    grpcmmo::auth::v1::CharacterSummary* character,
    std::string* error_message)
{
    const auto auth_channel =
        grpc::CreateChannel(config.auth_server_address,
                            grpc::InsecureChannelCredentials());
    auto auth_stub = grpcmmo::auth::v1::AuthService::NewStub(auth_channel);

    grpc::ClientContext list_context;
    grpcmmo::auth::v1::ListCharactersRequest list_request;
    list_request.set_account_access_token(login_response.account_access_token());
    list_request.set_realm_id(config.realm_id);
    grpcmmo::auth::v1::ListCharactersResponse list_response;
    const grpc::Status list_status =
        auth_stub->ListCharacters(&list_context, list_request, &list_response);
    if (!list_status.ok())
    {
        if (error_message != nullptr)
        {
            *error_message = "list characters failed: " + list_status.error_message();
        }
        return false;
    }

    for (const auto& item : list_response.characters())
    {
        if (item.name() == config.character_name)
        {
            *character = item;
            return true;
        }
    }

    grpc::ClientContext create_context;
    grpcmmo::auth::v1::CreateCharacterRequest create_request;
    create_request.set_account_access_token(login_response.account_access_token());
    create_request.set_realm_id(config.realm_id);
    create_request.set_name(config.character_name);
    grpcmmo::auth::v1::CreateCharacterResponse create_response;
    const grpc::Status create_status =
        auth_stub->CreateCharacter(&create_context, create_request, &create_response);
    if (!create_status.ok())
    {
        if (error_message != nullptr)
        {
            *error_message = "create character failed: " +
                             create_status.error_message();
        }
        return false;
    }

    *character = create_response.character();
    return true;
}

bool GrpcSessionClient::OpenSessionStream(
    const ClientConnectionConfig& config,
    const grpcmmo::auth::v1::CharacterSummary& character,
    const grpcmmo::auth::v1::CreateSessionGrantResponse& grant_response,
    std::string* error_message)
{
    const auto session_channel =
        grpc::CreateChannel(ToAddress(grant_response.endpoint()),
                            grpc::InsecureChannelCredentials());
    session_stub_ = grpcmmo::session::v1::SessionService::NewStub(session_channel);
    session_context_ = std::make_unique<grpc::ClientContext>();
    session_stream_ = session_stub_->OpenSession(session_context_.get());
    if (!session_stream_)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to open gameplay stream";
        }
        session_context_.reset();
        session_stub_.reset();
        return false;
    }

    grpcmmo::session::v1::ClientMessage begin_message;
    auto* begin = begin_message.mutable_begin_session();
    begin->set_session_token(grant_response.session_token());
    begin->set_character_id(character.character_id());
    begin->set_requested_zone_id(character.zone_id());
    if (!session_stream_->Write(begin_message))
    {
        if (error_message != nullptr)
        {
            *error_message = "server closed stream before begin_session";
        }
        session_stream_.reset();
        session_context_.reset();
        session_stub_.reset();
        return false;
    }

    open_.store(true, std::memory_order_relaxed);
    return true;
}

void GrpcSessionClient::StartReader()
{
    reader_thread_ = std::thread([this]()
                                 {
                                     grpcmmo::session::v1::ServerMessage message;
                                     while (session_stream_ && session_stream_->Read(&message))
                                     {
                                         std::lock_guard<std::mutex> lock(queue_mutex_);
                                         pending_messages_.push_back(message);
                                     }
                                     open_.store(false, std::memory_order_relaxed);
                                 });
}
} // namespace grpcmmo::client
