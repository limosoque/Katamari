#pragma once
#include <DirectXMath.h>

struct Material
{
    DirectX::XMFLOAT4 Ambient = { 0.2f, 0.2f, 0.2f, 1.0f };
    DirectX::XMFLOAT4 Diffuse = { 0.8f, 0.8f, 0.8f, 1.0f };
    DirectX::XMFLOAT4 Specular = { 0.5f, 0.5f, 0.5f, 1.0f };
    float Shininess = 32.0f;

    ///rubber for ball
    static Material Rubber()
    {
        Material m;
        m.Ambient = { 0.2f, 0.2f, 0.2f, 1.0f };
        m.Diffuse = { 0.85f, 0.85f, 0.85f, 1.0f };
        m.Specular = { 0.1f, 0.1f, 0.1f, 1.0f };
        m.Shininess = 8.0f;
        return m;
    }

    ///Matte ground
    static Material Ground()
    {
        Material m;
        m.Ambient = { 0.3f, 0.3f, 0.3f, 1.0f };
        m.Diffuse = { 0.7f, 0.7f, 0.7f, 1.0f };
        m.Specular = { 0.0f, 0.0f, 0.0f, 1.0f };
        m.Shininess = 1.0f;
        return m;
    }

    ///shiny plastic 
    static Material Plastic()
    {
        Material m;
        m.Ambient = { 0.2f, 0.2f, 0.2f, 1.0f };
        m.Diffuse = { 0.7f, 0.7f, 0.7f, 1.0f };
        m.Specular = { 0.9f, 0.9f, 0.9f, 1.0f };
        m.Shininess = 64.0f;
        return m;
    }

    ///Silver or latex exexe
    static Material Metal()
    {
        Material m;
        m.Ambient = { 0.23125f, 0.23125f, 0.23125f, 1.0f };
        m.Diffuse = { 0.2775f, 0.2775f,  0.2775f, 1.0f };
        m.Specular = { 0.773911f, 0.773911f, 0.773911f, 1.0f };
        m.Shininess = 89.6f;
        return m;
    }

    ///wood for chair
    static Material Wood()
    {
        Material m;
        m.Ambient = { 0.25f, 0.20f, 0.15f, 1.0f };
        m.Diffuse = { 0.70f, 0.55f, 0.40f, 1.0f };
        m.Specular = { 0.15f, 0.10f, 0.05f, 1.0f };
        m.Shininess = 12.0f;
        return m;
    }

    ///for seashell
    static Material Ceramic()
    {
        Material m;
        m.Ambient = { 0.25f, 0.25f, 0.25f, 1.0f };
        m.Diffuse = { 0.80f, 0.80f, 0.80f, 1.0f };
        m.Specular = { 0.70f, 0.70f, 0.70f, 1.0f };
        m.Shininess = 96.0f;
        return m;
    }

    ///organic creeper and mouse
    static Material Organic()
    {
        Material m;
        m.Ambient = { 0.3f, 0.25f, 0.2f, 1.0f };
        m.Diffuse = { 0.75f, 0.65f, 0.55f, 1.0f };
        m.Specular = { 0.2f, 0.15f, 0.1f, 1.0f };
        m.Shininess = 16.0f;
        return m;
    }
};
