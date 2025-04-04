#include "hal_I2S.h"
#include  "hal_mmio.h"

void set_I2S_params(I2S_PARAMS* param) {
    set_I2S_params_manual(param->channel, param->tx_en, param->rx_en, param->bitdepth_tx, param->bitdepth_rx, param->clkgen, param->dacen, param->ws_len);
    set_I2S_clkdiv(param->channel, param->clkdiv);
    set_I2S_fp(param->channel, param->tx_fp, param->rx_fp);
    set_I2S_force_left(param->channel, param->tx_force_left, param->rx_force_left);
}

void set_I2S_params_manual(int channel, int tx_en, int rx_en, int bitdepth_tx, int bitdepth_rx, int clkgen, int dacen, int ws_len) {
    uint16_t params = reg_read16(I2S_CONFIG(channel));
    params = params | ((bitdepth_rx & 0x03) << 8);
    params = params | ((bitdepth_tx & 0x03) << 6);
    params = params | ((ws_len & 0x03) << 4);
    params = params | ((tx_en & 0x01) << 1);
    params = params | ((rx_en & 0x01) << 2);
    params = params | ((dacen & 0x01) << 3);
    params = params | ((clkgen & 0x01));

    reg_write16(I2S_CONFIG(channel), params);
}

void set_I2S_clkdiv(int channel, int clkdiv) {
    reg_write16(I2S_CLKDIV(channel), clkdiv);
}

void set_I2S_watermark(int channel, int watermark_tx, int watermark_rx) {
    reg_write8(I2S_TX_WATERMARK(channel), watermark_tx);
    reg_write8(I2S_RX_WATERMARK(channel), watermark_rx);
}

void set_I2S_en(int channel, int tx_en, int rx_en) {
    uint16_t params = reg_read16(I2S_CONFIG(channel));
    params = params | ((tx_en & 0x01) << 1);
    params = params | ((rx_en & 0x01) << 2);
    reg_write16(I2S_CONFIG(channel), params);
}

uint64_t read_I2S_rx(int channel, int left) {
    if (left)
        return reg_read64(I2S_RX_L(channel));
    else
        return reg_read64(I2S_RX_R(channel));
}

void write_I2S_tx(int channel, int left, uint64_t data) {
    if (left)
        return reg_write64(I2S_TX_L(channel), data);
    else
        return reg_write64(I2S_TX_R(channel), data);
}

uint64_t write_I2S_tx_DMA(int channel, int dma_num, int length, uint64_t* read_addr, int left, int poll) {
    if (left) {
        //printf("Writing to left queue\n");
        set_DMAP(dma_num, read_addr, I2S_TX_L(channel), I2S_WATERMARK_TX_L(channel), 8, 8, length, 3, poll);
    } else {
        //printf("Writing to right queue\n");
        set_DMAP(dma_num, read_addr, I2S_TX_R(channel), I2S_WATERMARK_TX_R(channel), 8, 8, length, 3, poll);
    }
    start_DMA(dma_num);
    return 0;
}


uint64_t read_I2S_rx_DMA(int channel, int dma_num, int length, uint64_t* write_addr, int left, int poll) {
    if (left)
        set_DMAP(dma_num, I2S_RX_L(channel), write_addr, I2S_WATERMARK_RX_L(channel), 0, 8, length, 3, poll);
    else
        set_DMAP(dma_num, I2S_RX_R(channel), write_addr, I2S_WATERMARK_RX_R(channel), 0, 8, length, 3, poll);
    start_DMA(dma_num);
    return 0;
}

void set_I2S_fp(int channel, int tx_fp, int rx_fp) {
    uint16_t params = reg_read16(I2S_CONFIG(channel));
    params = params | ((rx_fp & 0x01) << 11);
    params = params | ((tx_fp & 0x01) << 10);
    reg_write16(I2S_CONFIG(channel), params);
}

void set_I2S_force_left(int channel, int tx_force_left, int rx_force_left) {
    uint16_t params = reg_read16(I2S_CONFIG(channel));
    params = params | ((rx_force_left & 0x01) << 13);
    params = params | ((tx_force_left & 0x01) << 12);
    reg_write16(I2S_CONFIG(channel), params);
}