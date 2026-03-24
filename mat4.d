import std.math;

static import vec3;

float[4][4] identity() {
  float[4][4] result;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }

  return result;
}

float[4][4] mul(float[4][4] a, float[4][4] b) {
    float[4][4] temp;

    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        temp[i][j] = 
          a[i][0] * b[0][j] +
          a[i][1] * b[1][j] +
          a[i][2] * b[2][j] +
          a[i][3] * b[3][j];
      }
    }
    
    return temp;
}

float degToRad(float degrees) {
  return degrees * cast(float) PI / 180.0f;
}

float[4][4] rotate(float[4][4] in_, float angle, float[3] axis) {
  float a = degToRad(angle);
  float c = cos(a);
  float s = sin(a);
  
  float[3] norm_axis = vec3.normalize(axis);
    
  float x = norm_axis[0];
  float y = norm_axis[1];
  float z = norm_axis[2];
    
  float t = 1.0f - c;
    
  float[4][4] rot = [
    [
      t * x * x + c,
      t * x * y - s * z,
      t * x * z + s * y,
      0.0f
    ],
    [
      t * x * y + s * z,
      t * y * y + c,
      t * y * z - s * x,
      0.0f
    ],
    [
      t * x * z - s * y,
      t * y * z + s * x,
      t * z * z + c,
      0.0f
    ],
    [
      0.0f,
      0.0f,
      0.0f,
      1.0f
    ]
  ];
    
  return mul(rot, in_);
}

float[4][4] lookAt(
  float[3] eye,
  float[3] center,
  float[3] up
) {
  float[3] f, s, u;
  
  f = vec3.sub(center, eye);
  f = vec3.normalize(f);
  
  s = vec3.cross(f, up);
  s = vec3.normalize(s);
  
  u = vec3.cross(s, f);
  
  return [
    [s[0], u[0], -f[0], 0.0f],
    [s[1], u[1], -f[1], 0.0f],
    [s[2], u[2], -f[2], 0.0f],
    [-vec3.dot(s, eye), -vec3.dot(u, eye), vec3.dot(f, eye), 1.0f]
  ];
}

float[4][4] perspective(float fovy_degrees, float aspect, float near, float far) {
  float f = 1.0f / tan(
    degToRad(fovy_degrees) / 2.0f
  );
  float nf = 1.0f / (near - far);
  
  float[4][4] result = [
    [
      f / aspect,
      0.0f,
      0.0f,
      0.0f
    ],
    [
      0.0f,
      f,
      0.0f,
      0.0f
    ],
    [
      0.0f,
      0.0f,
      (far + near) * nf,
      -1.0f
    ],
    [
      0.0f,
      0.0f,
      (2.0f * far * near) * nf,
      0.0f
    ]
  ];
    
  return result;
}
