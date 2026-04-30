#pragma once

#include <memory>
#include <string>

#include "Character.hpp"
#include "WorldActor.hpp"
#include "world/v1/replication.pb.h"

namespace grpcmmo::client
{
class ActorFactory
{
  public:
    [[nodiscard]] std::unique_ptr<PlanetActor> CreatePlanetActor(
        const std::string& entity_id, const std::string& display_name
    ) const
    {
        auto actor = std::make_unique<PlanetActor>();
        actor->SetEntityId(entity_id);
        actor->SetDisplayName(display_name);
        return actor;
    }

    [[nodiscard]] std::unique_ptr<StaticPropActor> CreateStaticPropActor(
        const std::string& entity_id, const std::string& display_name
    ) const
    {
        auto actor = std::make_unique<StaticPropActor>();
        actor->SetEntityId(entity_id);
        actor->SetDisplayName(display_name);
        return actor;
    }

    [[nodiscard]] std::unique_ptr<InteractivePropActor>
    CreateInteractivePropActor(
        const std::string& entity_id, const std::string& display_name
    ) const
    {
        auto actor = std::make_unique<InteractivePropActor>();
        actor->SetEntityId(entity_id);
        actor->SetDisplayName(display_name);
        return actor;
    }

    [[nodiscard]] std::unique_ptr<Actor> CreateReplicatedActor(
        const grpcmmo::world::v1::EntityPatch& patch
    ) const
    {
        std::unique_ptr<Actor> actor;
        switch (patch.metadata().kind())
        {
        case grpcmmo::world::v1::ENTITY_KIND_PLAYER:
            if (patch.metadata().controlled_entity())
            {
                actor = std::make_unique<PlayerCharacter>();
            }
            else
            {
                actor = std::make_unique<Character>();
            }
            break;
        case grpcmmo::world::v1::ENTITY_KIND_NPC:
            actor = std::make_unique<NpcCharacter>();
            break;
        case grpcmmo::world::v1::ENTITY_KIND_INTERACTIVE:
            actor = std::make_unique<InteractivePropActor>();
            break;
        case grpcmmo::world::v1::ENTITY_KIND_STATIC_PROP:
            actor = std::make_unique<StaticPropActor>();
            break;
        default:
            actor = std::make_unique<StaticPropActor>();
            break;
        }

        actor->SetEntityId(patch.entity_id());
        actor->SetDisplayName(patch.metadata().display_name());
        actor->SetReplicated(true);
        return actor;
    }
};
} // namespace grpcmmo::client
