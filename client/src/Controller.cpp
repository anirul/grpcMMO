#include "Controller.hpp"

namespace grpcmmo::client
{
    void Controller::Init()
    {
    }

    void Controller::End()
    {
        UnPossess();
    }

    void Controller::Tick(float /*delta_seconds*/)
    {
    }

    void Controller::Possess(Pawn* pawn)
    {
        pawn_ = pawn;
    }

    void Controller::UnPossess()
    {
        pawn_ = nullptr;
    }

    Pawn* Controller::GetPawn()
    {
        return pawn_;
    }

    const Pawn* Controller::GetPawn() const
    {
        return pawn_;
    }
} // namespace grpcmmo::client
