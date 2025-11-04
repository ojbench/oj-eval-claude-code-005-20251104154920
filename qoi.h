#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;
    uint8_t pre_r, pre_g, pre_b, pre_a;
    pre_r = 0u;
    pre_g = 0u;
    pre_b = 0u;
    pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        // Check for RUN operation
        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }
            continue;
        } else if (run > 0) {
            QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
            run = 0;
        }

        // Check for INDEX operation
        int index = QoiColorHash(r, g, b, a);
        if (history[index][0] == r && history[index][1] == g &&
            history[index][2] == b && history[index][3] == a) {
            QoiWriteU8(QOI_OP_INDEX_TAG | index);
        } else {
            // Update history
            history[index][0] = r;
            history[index][1] = g;
            history[index][2] = b;
            history[index][3] = a;

            // Check for DIFF operation
            int8_t dr = r - pre_r;
            int8_t dg = g - pre_g;
            int8_t db = b - pre_b;

            if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1 && a == pre_a) {
                QoiWriteU8(QOI_OP_DIFF_TAG | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2));
            } else {
                // Check for LUMA operation
                int8_t dr_luma = dr - dg;
                int8_t db_luma = db - dg;

                if (dg >= -32 && dg <= 31 && dr_luma >= -8 && dr_luma <= 7 &&
                    db_luma >= -8 && db_luma <= 7 && a == pre_a) {
                    QoiWriteU8(QOI_OP_LUMA_TAG | (dg + 32));
                    QoiWriteU8(((dr_luma + 8) << 4) | (db_luma + 8));
                } else {
                    // Use RGB or RGBA operation
                    if (a == pre_a) {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    } else {
                        QoiWriteU8(QOI_OP_RGBA_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                        QoiWriteU8(a);
                    }
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {

        uint8_t tag = QoiReadU8();

        if (tag == QOI_OP_RGB_TAG) {
            r = QoiReadU8();
            g = QoiReadU8();
            b = QoiReadU8();
        } else if (tag == QOI_OP_RGBA_TAG) {
            r = QoiReadU8();
            g = QoiReadU8();
            b = QoiReadU8();
            a = QoiReadU8();
        } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
            int index = tag & 0x3f;
            r = history[index][0];
            g = history[index][1];
            b = history[index][2];
            a = history[index][3];
        } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
            int8_t dr = ((tag >> 4) & 0x03) - 2;
            int8_t dg = ((tag >> 2) & 0x03) - 2;
            int8_t db = (tag & 0x03) - 2;
            r = pre_r + dr;
            g = pre_g + dg;
            b = pre_b + db;
        } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
            int8_t dg = (tag & 0x3f) - 32;
            uint8_t second_byte = QoiReadU8();
            int8_t dr = ((second_byte >> 4) & 0x0f) - 8;
            int8_t db = (second_byte & 0x0f) - 8;
            int8_t dr_full = dr + dg;
            int8_t db_full = db + dg;
            r = pre_r + dr_full;
            g = pre_g + dg;
            b = pre_b + db_full;
        } else if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
            int run_length = (tag & 0x3f) + 1;
            for (int j = 0; j < run_length; ++j) {
                QoiWriteU8(r);
                QoiWriteU8(g);
                QoiWriteU8(b);
                if (channels == 4) QoiWriteU8(a);

                // Update history for each pixel in the run
                int index = QoiColorHash(r, g, b, a);
                history[index][0] = r;
                history[index][1] = g;
                history[index][2] = b;
                history[index][3] = a;

                if (j < run_length - 1) {
                    i++; // Skip the next iteration since we're handling multiple pixels
                }
            }
            pre_r = r;
            pre_g = g;
            pre_b = b;
            pre_a = a;
            continue;
        }

        // Update history
        int index = QoiColorHash(r, g, b, a);
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
