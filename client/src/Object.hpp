#pragma once

namespace grpcmmo::client
{
class Object
{
public:
    virtual ~Object() = default;

    virtual void Init() = 0;
    virtual void End() = 0;
    virtual void Tick(float delta_seconds) = 0;
};
} // namespace grpcmmo::client
