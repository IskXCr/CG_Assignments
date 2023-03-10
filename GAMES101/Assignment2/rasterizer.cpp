// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}

// Return TRUE if the vector P0X is on the right hand side of the vector P0P1
static inline bool testCrossProduct(float x, float y, float _p0x, float _p0y, float _p1x, float _p1y)
{
    float v1x = x - _p0x;
    float v1y = y - _p0y;
    float v2x = _p1x - _p0x;
    float v2y = _p1y - _p0y;
    return (v1x * v2y - v2x * v1y) > 0;
}

static bool insideTriangle(float x, float y, const Vector3f* _v)
{   
    // TODO : Implement this function to check if the point (x, y) is inside the triangle represented by _v[0], _v[1], _v[2]

    bool flag = false;
    
    flag = testCrossProduct(x, y, _v[0].x(), _v[0].y(), _v[1].x(), _v[1].y());
    if (flag != testCrossProduct(x, y, _v[1].x(), _v[1].y(), _v[2].x(), _v[2].y()))
        return false;
    if (flag != testCrossProduct(x, y, _v[2].x(), _v[2].y(), _v[0].x(), _v[0].y()))
        return false;

    return true;
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        // TODO: Delete DEBUG info
        std::cout << "Z-Coordinate before transformation: " << buf[i[0]].z() << "\n"; 
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        // TODO: Delete DEBUG info
        std::cout << "Z-Coordinate after transformation: " << v[0].z() << "\n"; 
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }
        // TODO: Delete DEBUG info
        std::cout << "Z-Coordinate after expansion: " << v[0].z() << "\n"; 

        for (int i = 0; i < 3; ++i)
        {
            // Why three times?
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();
    
    // TODO : Find out the bounding box of current triangle.
    auto limit = std::numeric_limits<float>();
    float fxmin = limit.max();
    float fxmax = limit.min();
    float fymin = limit.max();
    float fymax = limit.min();
    for (auto &vec : t.v)
    {
        fxmin = std::min(vec.x(), fxmin);
        fxmax = std::max(vec.x(), fxmax);
        fymin = std::min(vec.y(), fymin);
        fymax = std::max(vec.y(), fymax);
    }

    int xmin = std::max(0, std::min((int)std::ceil(fxmin), width));
    int xmax = std::min(width, std::max((int)std::floor(fxmax), 0));
    int ymin = std::max(0, std::min((int)std::ceil(fymin), height));
    int ymax = std::min(height, std::max((int)std::floor(fymax), 0));

    // std::cout << "Triangle Coordinate:\n" << t.v[0] << ", \n" << t.v[1] << ", \n" << t.v[2] << "\n";
    // Eigen::Vector3f _color = t.color[0] * 255;
    // std::cout << "Triangle color\n" << _color << "\n";

    // iterate through the pixel and find if the current pixel is inside the triangle
    // If so, use the following code to get the interpolated z value.

    // auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
    // float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
    // float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
    // z_interpolated *= w_reciprocal;

    // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.

    for (int x = xmin; x < xmax; ++x)
    {
        for (int y = ymin; y < ymax; ++y)
        {
            // Procedure:
            // 1. Update buffer on each individual subpixel.
            // 2. Mix and get the result on the entire pixel.
            bool flag = false;
            for (int _xoffset = 0; _xoffset < 2; _xoffset += 1)
            {
                for (int _yoffset = 0; _yoffset < 2; _yoffset += 1)
                {
                    float _xbase = x + 0.25f + _xoffset * 0.5f;
                    float _ybase = y + 0.25f + _yoffset * 0.5f;
                    if (insideTriangle(_xbase, _ybase, t.v))
                    {
                        auto[alpha, beta, gamma] = computeBarycentric2D(_xbase, _ybase, t.v);
                        float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                        float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                        z_interpolated *= w_reciprocal;
                        if (z_interpolated < depth_buf_msaa_4x[get_index_msaa_4x(x, y, _xoffset, _yoffset)])
                        {
                            flag = true;
                            depth_buf_msaa_4x[get_index_msaa_4x(x, y, _xoffset, _yoffset)] = z_interpolated;
                            set_pixel_msaa_4x(x, y, _xoffset, _yoffset, t.getColor());
                        }
                    }
                }
            }

            if (flag) // If update occurs
            {
                Eigen::Vector3f color(0.0f, 0.0f, 0.0f);
                for (int _xoffset = 0; _xoffset < 2; _xoffset += 1)
                {
                    for (int _yoffset = 0; _yoffset < 2; _yoffset += 1)
                    {
                        color += frame_buf_msaa_4x[get_index_msaa_4x(x, y, _xoffset, _yoffset)];
                    }
                }
                color /= 4;
                set_pixel(Eigen::Vector3f(x, y, 0.0f), color);
            }
        }
    }
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }

    // MSAA 4x
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf_msaa_4x.begin(), frame_buf_msaa_4x.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf_msaa_4x.begin(), depth_buf_msaa_4x.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);

    // MSAA 4x
    frame_buf_msaa_4x.resize(w * h * 4);
    depth_buf_msaa_4x.resize(w * h * 4);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

// MSAA 4x
int rst::rasterizer::get_index_msaa_4x(int x, int y, int xoffset, int yoffset)
{
    return (height*2-1-y*2-yoffset)*width*2 + x*2 + xoffset;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;
}

void rst::rasterizer::set_pixel_msaa_4x(int x, int y, int xoffset, int yoffset, const Eigen::Vector3f &color)
{
    auto ind = (height*2-1-y*2-yoffset)*width*2 + x*2 + xoffset;
    frame_buf_msaa_4x[ind] = color;
}

// clang-format on