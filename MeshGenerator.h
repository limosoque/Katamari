#pragma once
#include "ObjLoader.h"

class MeshGenerator
{
public:
    /// UV-sphere with the given radius.
    /// stacks  — number of horizontal rings (latitude), minimum 2.
    /// slices  — number of vertical segments (longitude), minimum 3.
    /// BoundingRadius is set to radius.
    static MeshData CreateSphere(float radius = 1.0f,
                                 int   stacks  = 24,
                                 int   slices  = 24);
private:
    static void ComputeBoundingRadius(MeshData& data);
};
