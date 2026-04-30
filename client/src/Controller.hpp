#pragma once

#include "Object.hpp"

namespace grpcmmo::client
{
class Pawn;

class Controller : public Object
{
  public:
    void Init() override;
    void End() override;
    void Tick(float delta_seconds) override;

    virtual void Possess(Pawn* pawn);
    virtual void UnPossess();

    [[nodiscard]] Pawn* GetPawn();
    [[nodiscard]] const Pawn* GetPawn() const;

  private:
    Pawn* pawn_ = nullptr;
};
} // namespace grpcmmo::client
