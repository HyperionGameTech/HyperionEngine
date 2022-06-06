#include "triangle.h"
#include "math_util.h"

namespace hyperion {
Triangle::Triangle()
{
}

Triangle::Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2)
    : m_points({ v0, v1, v2 })
{
}

Triangle::Triangle(const Triangle &other)
    : m_points(other.m_points)
{
}

Triangle Triangle::operator*(const Matrix4 &mat) const
{
    return Triangle(
        GetPoint(0) * mat,
        GetPoint(1) * mat,
        GetPoint(2) * mat
    );
}

Triangle &Triangle::operator*=(const Matrix4 &mat)
{
    for (auto &pt : m_points) {
        pt *= mat;
    }

    return *this;
}

Triangle Triangle::operator*(const Transform &transform) const
{
    return operator*(transform.GetMatrix());
}

Triangle &Triangle::operator*=(const Transform &transform)
{
    return operator*=(transform.GetMatrix());
}

Vertex &Triangle::Closest(const Vector3 &vec)
{
    float distances[3];
    int shortest_index = 0;

    for (int i = 0; i < 3; i++) {
        distances[i] = m_points[i].GetPosition().DistanceSquared(vec);

        if (i != 0) {
            if (distances[i] < distances[shortest_index]) {
                shortest_index = i;
            }
        }
    }

    return m_points[shortest_index];
}

const Vertex &Triangle::Closest(const Vector3 &vec) const
{
    return const_cast<Triangle *>(this)->Closest(vec);
}

bool Triangle::IntersectRay(const Ray &ray, RayTestResults &out) const
{
    float t, u, v;

    Vector3 v0v1 = GetPoint(1).GetPosition() - GetPoint(0).GetPosition();
	Vector3 v0v2 = GetPoint(2).GetPosition() - GetPoint(0).GetPosition();
	Vector3 pvec = Vector3(ray.direction).Cross(v0v2);

	float det = v0v1.Dot(pvec);

	// ray and triangle are parallel if det is close to 0
	if (std::fabs(det) < MathUtil::epsilon<float>) return false;

	float invDet = 1.0 / det;

	Vector3 tvec = ray.position - GetPoint(0).GetPosition();
	u = tvec.Dot(pvec) * invDet;
	if (u < 0 || u > 1) return false;

	Vector3 qvec = Vector3(tvec).Cross(v0v1);
	v = ray.direction.Dot(qvec) * invDet;
	if (v < 0 || u + v > 1) return false;

	t = v0v2.Dot(qvec) * invDet;

    if (t > 0.0f) {
        out.AddHit({
            .hitpoint = ray.position + (ray.direction * t),
            .normal   = Vector3(v0v1).Cross(v0v2)
        });

        return true;
    }

    return false;
}
} // namespace
