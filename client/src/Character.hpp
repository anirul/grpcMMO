#pragma once

#include "Pawn.hpp"

namespace grpcmmo::client
{
class Character : public Pawn
{
  public:
    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "Character";
    }
};

class PlayerCharacter final : public Character
{
  public:
    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "PlayerCharacter";
    }
};

class NpcCharacter final : public Character
{
  public:
    [[nodiscard]] const char* GetActorClassName() const override
    {
        return "NpcCharacter";
    }
};
} // namespace grpcmmo::client
