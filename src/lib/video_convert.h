void convert16to8(const uint16_t* src, uint8_t** dst, size_t size)
{
    *dst = (uint8_t*)malloc(size * 2);
    for (size_t i = 0; i < size; ++i) {
        (*dst)[i*2] = (uint8_t)(src[i] << 8);
        (*dst)[i*2 + 1] = (uint8_t)(src[i] & 0xFF);  // Keep the lower 8 bits
    }
}

void convert8to16(const uint8_t* src, uint16_t** dst, size_t size)
{
    *dst = (uint16_t*)malloc(size * 2);
    for (size_t i = 0; i < size; i+=2) {
        (*dst)[i/2] = (uint16_t)(src[i] << 8 | src[i+1]);  // Keep the lower 8 bits
    }
}

void I420ToNV12_10bit(const uint16_t* srcY, const uint16_t* srcU, const uint16_t* srcV,
                      uint16_t** dstY, uint16_t** dstUV, int width, int height) {
    int frameSize = width * height;
    int chromaSize = (width / 2) * (height / 2);

    *dstY = (uint16_t*)malloc(frameSize * sizeof(uint16_t));
    *dstUV = (uint16_t*)malloc(2 * chromaSize * sizeof(uint16_t));

    // 1. Copy the Y plane (same for both I420 and NV12, except it's 10-bit)

    memcpy(*dstY, srcY, frameSize * sizeof(uint16_t));

    // 2. Interleave the U and V planes for NV12 (10-bit, 16-bit storage)
    for (int i = 0; i < chromaSize; ++i) {
        (*dstUV)[2 * i]     = srcU[i];  // U value (10 bits stored in 16-bit space)
        (*dstUV)[2 * i + 1] = srcV[i];  // V value (10 bits stored in 16-bit space)
    }
}

void NV12ToI420_10bit(const uint16_t* srcY, const uint16_t* srcUV,
                      uint16_t** dstY, uint16_t** dstU, uint16_t** dstV,
                      int width, int height) {
    int frameSize = width * height;
    int chromaSize = (width / 2) * (height / 2);

    *dstY = (uint16_t*)malloc(frameSize * sizeof(uint16_t));        // Y plane (I420, 10-bit)
    *dstU = (uint16_t*)malloc(chromaSize * sizeof(uint16_t));
        // U plane (I420, 10-bit)
    *dstV = (uint16_t*)malloc(chromaSize * sizeof(uint16_t));


    // 1. Copy the Y plane (same for both NV12 and I420, 10-bit)
    memcpy(*dstY, srcY, frameSize * sizeof(uint16_t));

    // 2. De-interleave the UV plane to separate U and V planes (10-bit, 16-bit storage)
    for (int i = 0; i < chromaSize; ++i) {
        (*dstU)[i] = srcUV[2 * i];      // U value
        (*dstV)[i] = srcUV[2 * i + 1];  // V value
    }
}