// NTSC brightness factors
const vec3 LuminanceNTSC = { 0.298999994993, 0.587000012398, 0.114000000060 };

////////////////////////////////// YUV / RGB //////////////////////////////////////////////////

const mat3 RGB2YUV = mat3(0.298999994993, 0.499812960625, -0.168635994196,
                          0.587000012398, -0.418531000614, -0.331068009138,
                          0.114000000060, -0.081281989813, 0.499704003334);

//const mat4 YUV2RGB =
//mat4(1.000000000000, 1.000000000000, 1.000000000000, 0.000000000000,
//1.402999997139, -0.713999986649, 0.000000000000, 0.000000000000,
//0.000000000000, -0.343999981880, 1.773000001907, 0.000000000000,
//-0.701499998569, 0.528999984264, -0.886500000954, 1.000000000000);

////////////////////////////////// YCoCg / RGB //////////////////////////////////////////////////

const mat3 RGB2YCoCg = mat3(0.25, 0.5, -0.25, 0.5, 0.0, 0.5, 0.25, -0.5, -0.25);

const mat3 YCoCg2RGB = mat3(1.0, 1.0, 1.0, 1.0, 0.0, -1.0, -1.0, 1.0, -1.0);