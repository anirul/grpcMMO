#pragma once

#include <chrono>

#include "AssetBootstrap.hpp"
#include "ClientWorld.hpp"
#include "GrpcSessionClient.hpp"

namespace frame
{
} // namespace frame

namespace grpcmmo::client
{
    class Client
    {
      public:
        int Run(int argc, char** argv);

      private:
        void LoadFlags();
        void PumpNetworkMessages();
        bool Tick(float delta_seconds);
        bool SendMoveIfDue();

        ClientConnectionConfig connection_config_;
        glm::uvec2 window_size_{1280u, 720u};
        std::chrono::milliseconds move_send_interval_{50};
        std::chrono::duration<float> auto_move_duration_{0.0f};
        std::chrono::steady_clock::time_point last_frame_time_{};
        std::chrono::steady_clock::time_point last_move_sent_at_{};
        bool running_ = true;
        AssetBootstrap asset_bootstrap_{};
        ClientWorld client_world_{};
        GrpcSessionClient session_;
    };
} // namespace grpcmmo::client
