#ifndef MATERIALH
#define MATERIALH
struct hit_record;

#include "ray.hpp"
#include "hitable.hpp"
#include "texture.hpp"
#ifndef PI
# define PI 3.14159265358979323846
#endif // !PI


#define RANDVEC3 vec3(curand_uniform(local_rand_state),curand_uniform(local_rand_state),curand_uniform(local_rand_state))

__device__ vec3 random_in_unit_sphere(curandState* local_rand_state) {
	vec3 p;
	do {
		p = 2.0f * RANDVEC3 - vec3(1, 1, 1);
	} while (p.length_squared() >= 1.0f);
	return p;
}

class material {
public:
	__device__ virtual int scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered, curandState* local_rand_state) const = 0;
	_texture *albedo;
};

class Emit : public material {
public:
	__device__ Emit(_texture* emit) : albedo(emit) {};
	__device__ virtual int scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered, curandState* local_rand_state) const {
		vec3 target = rec.p + rec.normal + random_in_unit_sphere(local_rand_state);
		scattered = ray(rec.p, target - rec.p, r_in.time());
		attenuation = albedo->value(rec.u,rec.v, rec.p);
		return 2;
	}
	_texture* albedo;
};

class lambertian : public material {
public:
	__device__ lambertian(_texture* a) : albedo(a) {};
	__device__ virtual int scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered, curandState* local_rand_state) const {
		vec3 target = rec.p + rec.normal + random_in_unit_sphere(local_rand_state);
		scattered = ray(rec.p, target - rec.p, r_in.time());
		attenuation = albedo->value(rec.u,rec.v,rec.p);
		return 1;
	}

	_texture* albedo;
};

__device__ vec3 reflect(const vec3& v, const vec3& n) {
	return v - 2 * dot(v, n) * n;
}

class metal : public material {
public:
	__device__ metal(_texture* a, float f) : albedo(a) { if (f < 1) fuzz = f; else fuzz = 1; }
	__device__ virtual int scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered, curandState* local_rand_state) const {
		vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
		scattered = ray(rec.p, reflected + fuzz*random_in_unit_sphere(local_rand_state), r_in.time());
		attenuation = albedo->value(rec.u,rec.v,rec.p);
		return (dot(scattered.direction(), rec.normal) > 0) ? 1 : 0;
	}

	_texture* albedo;
	float fuzz;
};

// n sin(theta) = n' sin(theta')
__device__ bool refract(const vec3& v, const vec3& n, float ni_over_nt, vec3& refracted) {
	vec3 uv = unit_vector(v);
	float dt = dot(uv, n);
	float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1 - dt * dt);
	if (discriminant > 0) {
		refracted = ni_over_nt * (uv - n * dt) - sqrt(discriminant) * n;
		return true;
	}
	else
		return false;
}

__device__ float schlick(float cosine, float ref_idx) {
	float r0 = (1 - ref_idx) / (1 + ref_idx);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

class dielectric : public material {
public:
	__device__ dielectric(float ri) : ref_idx(ri) {}
	__device__ virtual int scatter(const ray& r_in, const hit_record& rec, vec3& attenuation, ray& scattered, curandState* local_rand_state) const {
		vec3 outward_normal;
		vec3 reflected = reflect(r_in.direction(), rec.normal);
		float ni_over_nt;
		attenuation = vec3(1.0, 1.0, 1.0);
		vec3 refracted;
		float reflect_prob;
		float cosine;
		if (dot(r_in.direction(), rec.normal) > 0) {
			outward_normal = -rec.normal;
			ni_over_nt = ref_idx;
			cosine = ref_idx * dot(r_in.direction(), rec.normal) / r_in.direction().length();
		}
		else {
			outward_normal = rec.normal;
			ni_over_nt = 1.0 / ref_idx;
			cosine = -dot(r_in.direction(), rec.normal) / r_in.direction().length();
		}
		if (refract(r_in.direction(), outward_normal, ni_over_nt, refracted)) {
			reflect_prob = schlick(cosine, ref_idx);
		}
		else {
			reflect_prob = 1.0;
		}
		if (curand_uniform(local_rand_state) < reflect_prob) {
			scattered = ray(rec.p, reflected, r_in.time());
		}
		else {
			scattered = ray(rec.p, refracted, r_in.time());
		}
		return 1;
	}
	float ref_idx;
	_texture* albedo;
};

#endif