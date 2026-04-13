#pragma once
#include "ObjLoader.h"   // reuses MeshData / MeshVertex

/// Procedural mesh generation utilities.
/// All methods return a MeshData ready to be uploaded via Mesh::Upload().
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

    /// Axis-aligned box centred at the origin.
    static MeshData CreateBox(float halfX = 0.5f,
                              float halfY = 0.5f,
                              float halfZ = 0.5f);

    /// Cylinder (with end caps) centred at the origin, axis along Y.
    static MeshData CreateCylinder(float radius     = 0.5f,
                                   float halfHeight = 1.0f,
                                   int   slices     = 20);

    /// Cone (with a bottom cap) centred at the origin, tip at +Y, base at -Y.
    static MeshData CreateCone(float radius     = 0.5f,
                               float halfHeight = 1.0f,
                               int   slices     = 20);

private:
    static void ComputeBoundingRadius(MeshData& data);
};
