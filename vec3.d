import std.math;

float dot(float[3] a, float[3] b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float[3] normalize(float[3] in_) {
  float len = sqrt(
    in_[0] * in_[0] +
    in_[1] * in_[1] +
    in_[2] * in_[2]
  );

  if (len > 0.0f) {
    float inv_len = 1.0f / len;

    return [
      in_[0] * inv_len,
      in_[1] * inv_len,
      in_[2] * inv_len
    ];
  }
  
  return [0.0f, 0.0f, 0.0f];
}

float[3] cross(
  float[3] a,
  float[3] b
) {
  return [
    a[1] * b[2] - a[2] * b[1],
    a[2] * b[0] - a[0] * b[2],
    a[0] * b[1] - a[1] * b[0]
  ];
}

float[3] sub(
  float[3] a,
  float[3] b
) {
  return [
    a[0] - b[0],
    a[1] - b[1],
    a[2] - b[2]
  ];
}
