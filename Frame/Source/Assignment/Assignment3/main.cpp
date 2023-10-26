#include <iostream>
#include <opencv2/opencv.hpp>

#include "global.hpp"
#include "rasterizer.hpp"
#include "Triangle.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "OBJ_Loader.h"

#include <filesystem>

std::string get_asset_path(std::string relativePath)
{
    return (std::filesystem::path(FRAME_ASSET_PATH) / std::move(relativePath)).generic_string();
}

Eigen::Matrix4f get_view_matrix(const Eigen::Vector3f &eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1,0,0,-eye_pos[0],
                 0,1,0,-eye_pos[1],
                 0,0,1,-eye_pos[2],
                 0,0,0,1;

    view = translate*view;

    return view;
}

Eigen::Matrix4f get_model_matrix(float angle)
{
    Eigen::Matrix4f rotation;
    angle = angle * MY_PI / 180.f;
    rotation << cos(angle), 0, sin(angle), 0,
                0, 1, 0, 0,
                -sin(angle), 0, cos(angle), 0,
                0, 0, 0, 1;

    Eigen::Matrix4f scale;
    scale << 2.5, 0, 0, 0,
              0, 2.5, 0, 0,
              0, 0, 2.5, 0,
              0, 0, 0, 1;

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1;

    return translate * rotation * scale;
}

Eigen::Matrix4f get_projection_matrix(float fov, float aspect, float near, float far)
{
    // �޸���� bug
    near = -near;
    far = -far;

    // �� frustum �Ķ���� top �� right
    float top = std::tan(fov * 0.5f * MY_PI / 180.0f) * std::abs(near);
    float right = aspect * top;

    // �������ʱ��λ���뷽��� bottom = -top �� left = -right
    float bottom = -top;
    float left = -right;

    Eigen::Matrix4f Mp2o;
    Mp2o <<
        near, 0.0f, 0.0f, 0.0f,
        0.0f, near, 0.0f, 0.0f,
        0.0f, 0.0f, near + far, -near * far,
        0.0f, 0.0f, 1.0f, 0.0f;

    Eigen::Matrix4f Mtrans;
    Mtrans <<
        1.0f, 0.0f, 0.0f, (right + left) * -0.5f,
        0.0f, 1.0f, 0.0f, (top + bottom) * -0.5f,
        0.0f, 0.0f, 1.0f, (near + far) * -0.5f,
        0.0f, 0.0f, 0.0f, 1.0f;

    Eigen::Matrix4f Mrotate;
    Mrotate <<
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, 2.0f / (near - far), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;

    Eigen::Matrix4f Morthographic = Mrotate * Mtrans;
    Eigen::Matrix4f Mprojection = Morthographic * Mp2o;

    return Mprojection;
}

Eigen::Vector3f vertex_shader(const vertex_shader_payload& payload)
{
    return payload.position;
}

Eigen::Vector3f normal_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f return_color = (payload.normal.head<3>().normalized() + Eigen::Vector3f(1.0f, 1.0f, 1.0f)) / 2.f;
    Eigen::Vector3f result;
    result << return_color.x() * 255, return_color.y() * 255, return_color.z() * 255;
    return result;
}

static Eigen::Vector3f reflect(const Eigen::Vector3f& vec, const Eigen::Vector3f& axis)
{
    auto costheta = vec.dot(axis);
    return (2 * costheta * axis - vec).normalized();
}

struct light
{
    Eigen::Vector3f position;
    Eigen::Vector3f intensity;
};

Eigen::Vector3f phong_fragment_shader(const fragment_shader_payload& payload)
{
    static Eigen::Vector3f ka = Eigen::Vector3f(0.005f, 0.005f, 0.005f);
    Eigen::Vector3f kd = payload.color;
    static Eigen::Vector3f ks = Eigen::Vector3f(0.7937f, 0.7937f, 0.7937f);

    static std::vector<light> lights = {
        light{{20.0f, 20.0f, 20.0f}, {500.0f, 500.0f, 500.0f}},
        light{{-20.0f, 20.0f, 0.0f}, {500.0f, 500.0f, 500.0f}} };

    static Eigen::Vector3f amb_light_intensity{10.0f, 10.0f, 10.0f };
    static Eigen::Vector3f eye_pos{0.0f, 0.0f, 10.0f };

    constexpr static float p = 150.0f;

    Eigen::Vector3f normal = payload.normal.normalized();
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f viewDir = (eye_pos - point).normalized();

    Eigen::Vector3f color = { 0.0f, 0.0f, 0.0f };

    for (auto& light : lights)
    {
        Eigen::Vector3f lightDir = light.position - point;
        float distance2 = lightDir.dot(lightDir);
        lightDir.normalize();
        Eigen::Vector3f halfDir = (viewDir + lightDir).normalized();
        float NdotL = normal.dot(lightDir);
        float NdotH = normal.dot(halfDir);

        Eigen::Vector3f intensity = light.intensity / distance2;

        Eigen::Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
        Eigen::Vector3f diffuse = kd.cwiseProduct(intensity) * std::max(0.0f, NdotL);
        Eigen::Vector3f specular = ks.cwiseProduct(intensity) * std::pow(std::max(0.0f, NdotH), p);
        color += (ambient + diffuse + specular);
    }

    return color * 255.0f;
}

Eigen::Vector3f texture_fragment_shader(const fragment_shader_payload &payload)
{
    static constexpr float reciprocal = 1.0f / 255.0f;

    static Eigen::Vector3f ka = Eigen::Vector3f(0.005f, 0.005f, 0.005f);
    Eigen::Vector3f kd =
        payload.texture ?
        payload.texture->getColor(payload.tex_coords.x(), payload.tex_coords.y()) * reciprocal :
        payload.color;
    static Eigen::Vector3f ks = Eigen::Vector3f(0.7937f, 0.7937f, 0.7937f);

    static std::vector<light> lights = {
        light{{20.0f, 20.0f, 20.0f}, {500.0f, 500.0f, 500.0f}},
        light{{-20.0f, 20.0f, 0.0f}, {500.0f, 500.0f, 500.0f}} };

    static Eigen::Vector3f amb_light_intensity{ 10.0f, 10.0f, 10.0f };
    static Eigen::Vector3f eye_pos{ 0.0f, 0.0f, 10.0f };

    constexpr static float p = 150.0f;

    Eigen::Vector3f normal = payload.normal.normalized();
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f viewDir = (eye_pos - point).normalized();

    Eigen::Vector3f color = { 0.0f, 0.0f, 0.0f };

    for (auto &light : lights)
    {
        Eigen::Vector3f lightDir = light.position - point;
        float distance2 = lightDir.dot(lightDir);
        lightDir.normalize();
        Eigen::Vector3f halfDir = (viewDir + lightDir).normalized();
        float NdotL = normal.dot(lightDir);
        float NdotH = normal.dot(halfDir);

        Eigen::Vector3f intensity = light.intensity / distance2;

        Eigen::Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
        Eigen::Vector3f diffuse = kd.cwiseProduct(intensity) * std::max(0.0f, NdotL);
        Eigen::Vector3f specular = ks.cwiseProduct(intensity) * std::pow(std::max(0.0f, NdotH), p);
        color += (ambient + diffuse + specular);
    }

    return color * 255.0f;
}

Eigen::Vector3f bump_fragment_shader(const fragment_shader_payload &payload)
{
    constexpr static float kh = 0.2f;
    constexpr static float kn = 0.1f;

    Eigen::Vector3f normal = payload.normal.normalized();

    float x = normal.x();
    float y = normal.y();
    float z = normal.z();
    float sqrtx2pz2 = sqrt(x * x + z * z);

    Eigen::Vector3f t = Eigen::Vector3f{
        x * y / sqrtx2pz2,
        sqrtx2pz2,
        z * y / sqrtx2pz2 }.normalized();
    Eigen::Vector3f b = normal.cross(t).normalized();
    Eigen::Matrix3f TBN;
    TBN <<
        t.x(), b.x(), normal.x(),
        t.y(), b.y(), normal.y(),
        t.z(), b.z(), normal.z();

    float u = payload.tex_coords.x();
    float v = payload.tex_coords.y();
    float w = payload.texture->width;
    float h = payload.texture->height;

    float height = payload.texture->getColor(u, v).norm();
    float height_u = payload.texture->getColor(u + 1.0f / w, v).norm();
    float height_v = payload.texture->getColor(u, v + 1.0f / h).norm();

    float du = kh * kn * (height_u - height);
    float dv = kh * kn * (height_v - height);

    Eigen::Vector3f localNormal = Eigen::Vector3f{ -du, -dv, 1.0f };
    normal = (TBN * localNormal).normalized();

    return normal * 255.f;
}

Eigen::Vector3f displacement_fragment_shader(const fragment_shader_payload& payload)
{
    static Eigen::Vector3f ka = Eigen::Vector3f(0.005f, 0.005f, 0.005f);
    Eigen::Vector3f kd = payload.color;
    static Eigen::Vector3f ks = Eigen::Vector3f(0.7937f, 0.7937f, 0.7937f);

    static std::vector<light> lights = {
        light{{20.0f, 20.0f, 20.0f}, {500.0f, 500.0f, 500.0f}},
        light{{-20.0f, 20.0f, 0.0f}, {500.0f, 500.0f, 500.0f}} };

    static Eigen::Vector3f amb_light_intensity{ 10.0f, 10.0f, 10.0f };
    static Eigen::Vector3f eye_pos{ 0.0f, 0.0f, 10.0f };

    constexpr static float kh = 0.2f;
    constexpr static float kn = 0.1f;
    constexpr static float p = 150.0f;

    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal.normalized();
    Eigen::Vector3f viewDir = (eye_pos - point).normalized();

    point += kn * normal * payload.texture->getColor(payload.tex_coords.x(), payload.tex_coords.y()).norm();

    {
        float x = normal.x();
        float y = normal.y();
        float z = normal.z();
        float sqrtx2pz2 = sqrt(x * x + z * z);

        Eigen::Vector3f t = Eigen::Vector3f{
            x * y / sqrtx2pz2,
            sqrtx2pz2,
            z * y / sqrtx2pz2 }.normalized();
        Eigen::Vector3f b = normal.cross(t).normalized();
        Eigen::Matrix3f TBN;
        TBN <<
            t.x(), b.x(), normal.x(),
            t.y(), b.y(), normal.y(),
            t.z(), b.z(), normal.z();

        float u = payload.tex_coords.x();
        float v = payload.tex_coords.y();
        float w = payload.texture->width;
        float h = payload.texture->height;

        float height = payload.texture->getColor(u, v).norm();
        float height_u = payload.texture->getColor(u + 1.0f / w, v).norm();
        float height_v = payload.texture->getColor(u, v + 1.0f / h).norm();

        float du = kh * kn * (height_u - height);
        float dv = kh * kn * (height_v - height);

        Eigen::Vector3f localNormal = Eigen::Vector3f{ -du, -dv, 1.0f };
        normal = (TBN * localNormal).normalized();
    }

    Eigen::Vector3f color = { 0.0f, 0.0f, 0.0f };

    for (auto &light : lights)
    {
        Eigen::Vector3f pointToLight = light.position - point;
        float distance2 = pointToLight.dot(pointToLight);

        Eigen::Vector3f lightDir = pointToLight.normalized();
        Eigen::Vector3f halfDir = (viewDir + lightDir).normalized();

        float NdotL = normal.dot(lightDir);
        float NdotH = normal.dot(halfDir);

        Eigen::Vector3f intensity = light.intensity / distance2;

        Eigen::Vector3f ambient = ka.cwiseProduct(amb_light_intensity);
        Eigen::Vector3f diffuse = kd.cwiseProduct(intensity) * std::max(0.0f, NdotL);
        Eigen::Vector3f specular = ks.cwiseProduct(intensity) * std::pow(std::max(0.0f, NdotH), p);
        color += (ambient + diffuse + specular);
    }

    return color * 255.f;
}

int main()
{
    objl::Loader Loader;
    std::vector<Triangle *> TriangleList;
    Loader.LoadFile(get_asset_path("models/spot/spot_triangulated_good.obj"));
    for(auto &mesh : Loader.LoadedMeshes)
    {
        for(int i = 0; i < mesh.Vertices.size(); i += 3)
        {
            Triangle* t = new Triangle();
            for(int j = 0; j < 3; ++j)
            {
                t->setVertex(j, Vector4f(mesh.Vertices[i + j].Position.X, mesh.Vertices[i + j].Position.Y, mesh.Vertices[i + j].Position.Z, 1.0));
                t->setNormal(j, Vector3f(mesh.Vertices[i + j].Normal.X, mesh.Vertices[i + j].Normal.Y, mesh.Vertices[i + j].Normal.Z));
                t->setTexCoord(j, Vector2f(mesh.Vertices[i + j].TextureCoordinate.X, mesh.Vertices[i + j].TextureCoordinate.Y));
            }
            TriangleList.push_back(t);
        }
    }

    int key = 0;
    int frame_count = 0;
    float angle = 140.0f;
    rst::rasterizer r(700, 700);
    Eigen::Vector3f eye_pos = { 0.0f,0.0f,10.0f };

    // normal_fragment_shader
    // phong_fragment_shader
    // texture_fragment_shader
    // bump_fragment_shader
    // displacement_fragment_shader
    r.set_vertex_shader(vertex_shader);
    r.set_fragment_shader(displacement_fragment_shader);

    // r.set_texture(Texture(get_asset_path("models/spot/spot_texture.png")));
    r.set_texture(Texture(get_asset_path("models/spot/hmap.jpg")));

    while(key != 27)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

        r.draw(TriangleList);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

        cv::imshow("image", std::move(image));

        key = cv::waitKey(1);
        if (key == 'a' )
        {
            angle -= 1.0f;
        }
        else if (key == 'd')
        {
            angle += 1.0f;
        }
    }
    return 0;
}