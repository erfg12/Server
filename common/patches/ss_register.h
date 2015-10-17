#pragma warning( disable : 4005 )

#define E(x) encoders[x] = Encode_##x;
#define D(x) decoders[x] = Decode_##x;
