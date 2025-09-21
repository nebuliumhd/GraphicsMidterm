// Contains all of our uniforms

// Ensure that EACH variable in the uniform starts at a multiple of 16 bytes
struct MyUniforms { // Total size of the struct has to be a multiple of the alignment size of its largest field
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	color: vec4f,
	time: f32,
};

const pi = 3.14159265359;
@group(0) @binding(0) var<uniform> u_myUniforms: MyUniforms;

// The struct passed to the vertex assembler stage
struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
};

// Cannot directly send struct to fragment through C++, must return it from vertex shader
struct VertexOutput {
	@builtin(position) position: vec4f, // @builtin(position) is required by the rasterizer
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
};

fn makeOrthographicProj(ratio: f32, near: f32, far: f32, scale: f32) -> mat4x4f {
	return transpose(mat4x4f(
		1.0 / scale,      0.0,           0.0,                  0.0,
		    0.0,     ratio / scale,      0.0,                  0.0,
		    0.0,          0.0,      1.0 / (far - near), -near / (far - near),
		    0.0,          0.0,           0.0,                  1.0,
	));
}

fn makePerspectiveProj(ratio: f32, near: f32, far: f32, focalLength: f32) -> mat4x4f {
	let divides = 1.0 / (far - near);
	return transpose(mat4x4f(
		focalLength,         0.0,              0.0,               0.0,
		    0.0,     focalLength * ratio,      0.0,               0.0,
		    0.0,             0.0,         far * divides, -far * near * divides,
		    0.0,             0.0,              1.0,               0.0,
	));
}

@vertex
fn vs_main(v_in: VertexInput) -> VertexOutput {
	var v_out: VertexOutput;
	v_out.position =
		u_myUniforms.projectionMatrix *
		u_myUniforms.viewMatrix *
		u_myUniforms.modelMatrix *
		vec4f(
			v_in.position,
			1.0
		);
	v_out.normal = (u_myUniforms.modelMatrix * vec4f(v_in.normal, 0.0)).xyz;
	v_out.color = v_in.color;
	return v_out;
}

@fragment
fn fs_main(f_in: VertexOutput) -> @location(0) vec4f {
	let normal = normalize(f_in.normal);
	let lightColor1 = vec3f(1.0, 0.9, 0.6);
	let lightColor2 = vec3f(0.6, 0.9, 1.0);
	let lightDirection1 = vec3f(0.5, -0.9, 0.1);
	let lightDirection2 = vec3f(0.2, 0.4, 0.3);
	let shading1 = max(0.0, dot(lightDirection1, normal));
	let shading2 = max(0.0, dot(lightDirection2, normal));
	let shading = shading1 * lightColor1 + shading2 * lightColor2;
	let color = normal * shading;
	// Gamma correction (Not needed)
	// let linear_color = pow(color, vec3f(2.2));
	return vec4f(color, u_myUniforms.color.a);
}