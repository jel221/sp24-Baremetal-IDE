/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "chip_config.h"
#include <inttypes.h>


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN PUC */


/*
110 0001 1011
110 0001 1011
    1111 0011

| 15-14 |   13   |   12   |    11    |    10    |
  RSVD    RX_FL     TX_FL   RX_USE_F   TX_USE_F   
|  9-8  |  7-6  |   5-4   |    3    |   2   |   1   |
  RX_BD   TX_BD   WS_SIZE   DAC_EN    RX_EN   TX_EN
*/

#define CHANNEL 0
#include "btstory.h"
uint64_t* audio = (uint64_t*) story_bearly_wav;
#define AUDIO_LEN STORY_BEARLY_WAVE_LEN


uint16_t kernel[8] = {
  0x0000, 0x3C00, 0x0000, 0x3C00, 0x0000, 0x3C00, 0x0000, 0x3C00
};

void app_init() {
  uint64_t mhartid = READ_CSR("mhartid");

  printf("(BEGIN) On hart: %d", mhartid);

  set_I2S_params(CHANNEL, true, true, 3, 3, true, 0, 2);
  set_I2S_en(0, true, true);
  set_I2S_clkdiv(CHANNEL, 7); // hardcoded

  printf("Init done");
}

double compute_pi(int iterations) {
    double pi = 0.0;
    int sign = 1;

    for (long long i = 0; i < iterations; i++) {
        pi += sign * (1.0 / (2 * i + 1));
        sign = -sign;
    }

    pi *= 4;

    return pi;
}

void compare_dma() {
  printf("(TEST 1) Normal start");
  uint64_t normal_start = get_cycles();
  size_t counter;
  double pi;
  for (counter = 0; counter < 32; counter++)
    write_I2S_tx(CHANNEL, true, audio[counter]);
  //pi = compute_pi(10);

  for (; counter < 64; counter++)
    write_I2S_tx(CHANNEL, true, audio[counter]);
  //pi = compute_pi(10);

  for (; counter < 96; counter++)
    write_I2S_tx(CHANNEL, true, audio[counter]);
  //pi = compute_pi(10);

  for (; counter < 128; counter++)
    write_I2S_tx(CHANNEL, true, audio[counter]);
  //pi = compute_pi(10);

  uint64_t normal_end = get_cycles();

  printf("(TEST 1) Normal end");
  printf("Normal cycle count: %d", normal_end - normal_start);
 //printf("DEBUG: pi %f", pi);

  printf("(TEST 1) DMA start");
  uint64_t accel_start = get_cycles();
  
  write_I2S_tx_DMA(CHANNEL, 8, 32, audio + 0 , true, 100000);
  pi = compute_pi(10);
  write_I2S_tx_DMA(CHANNEL, 8, 32, audio + 32, true, 100000);
  pi = compute_pi(10);
  write_I2S_tx_DMA(CHANNEL, 8, 32, audio + 64, true, 100000);
  pi = compute_pi(10);
  write_I2S_tx_DMA(CHANNEL, 8, 32, audio + 96, true, 100000);
  pi = compute_pi(10);

  uint64_t accel_end = get_cycles();

  printf("(TEST 1) DMA end");
  printf("DMA cycle count: %d", accel_end - accel_start);
  //printf("DEBUG: pi %f", pi);
}
/* USER CODE END PUC */

void software_conv(int8_t *arr, size_t arr_len, short* kernel, size_t kernel_len, size_t dilation, short* output) {
  for (int i = 0; i < arr_len; i += 1) {
    
    output[i] = 0;
    for (int j = 0; j < kernel_len; j += 1) {
      int arr_index = i + j * dilation;
      int8_t item;
      if (arr_index >= arr_len) {
        item = 0;
      } else {
        item = arr[arr_index];
      }
      output[i] = f16_add(output[i], f16_mul(f16_from_int((int32_t)item), kernel[j]));
    }
  }
}


void compare_conv() {
  printf("(TEST 2) Normal start");
  uint64_t normal_start = get_cycles();
  
  uint16_t output_a[AUDIO_LEN];

  software_conv((int8_t*) audio, AUDIO_LEN, kernel, 8, 1, output_a);

  uint64_t normal_end = get_cycles();

  printf("(TEST 2) Normal end");
  printf("Normal cycle count: %d", normal_end - normal_start);

  printf("(TEST 2) Conv start");
  uint64_t accel_start = get_cycles();

  uint16_t output_b[AUDIO_LEN];
  
  reg_write64(CONV_BASE, audio);
  set_conv_params(120, 1, kernel);
  //write_conv_dma(1, 120, audio);
  start_conv();
  asm volatile("fence");

  uint64_t* output_b_long = (uint64_t*) output_b;
  
  for (int i = 0; i < AUDIO_LEN / 4; i++) {
    uint64_t temp = reg_read64(CONV_OUTPUT_ADDR);
    output_b_long[i] = temp;
  }
  
  uint64_t accel_end = get_cycles();

  // This is causing program to hang. WTF? printf("(TEST 2) Conv end");
  printf("Conv cycle count: %d", accel_end - accel_start);

  printf("(TEST 2) Verifying output");

  for (int i = 0; i < AUDIO_LEN; i++) {
    if (output_a[i] != output_b[i]) {
        printf("Expected %x, got %x", output_a[i], output_b[i]);
    }
  }

  printf("(TEST 2) Verifying output end");
}

// #define BASE_ADDR 0x08800000
// 
// #define INPUT_ADDR      0x08800000
// #define OUTPUT_ADDR     0x08800020
// #define KERNEL_ADDR     0x08800040
// #define STATUS_ADDR      0x0880006C
// #define START_ADDR      0x0880006C
// #define CLEAR_ADDR      0x0880006D
// #define LENGTH_ADDR     0x08800078
// #define DILATION_ADDR   0x0880007C
// #define DEQ_ADDR    0x0880008D
// #define ISFLOAT_ADDR    0x0880008E

//void simple_test() {
//    int8_t in_arr[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
//    uint32_t in_len[1] = {16};
//    uint16_t in_dilation[1] = {1};
//    uint16_t in_kernel[8] = {0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}; // {2, 0, 0, 0, 0, 0, 0, 0} in FP16                                    
//
//    reg_write8(CLEAR_ADDR, 1);
//    asm volatile("fence");
//    reg_write8(CLEAR_ADDR, 0);
//    printf("Test setup: ");
//    reg_write64(INPUT_ADDR, *((uint64_t*) (in_arr)));
//    //reg_write64(INPUT_ADDR, *((uint64_t*) (in_arr + 8)));
//
//    reg_write32(LENGTH_ADDR, 16);
//    reg_write16(DILATION_ADDR, in_dilation[0]);
//    reg_write8(ISFLOAT_ADDR, 0);
//    
//    reg_write64(KERNEL_ADDR, *((uint64_t*) in_kernel));         // 64 bits: 4 FP16s
//    //reg_write64(KERNEL_ADDR, *((uint64_t*) (in_kernel + 4)));   // 64 bits: 4 FP16s (Total 8)
//
//    asm volatile("fence");
//    printf("Starting: ");
//    reg_write8(START_ADDR, 1);
//
//    
//    printf("Input (INT8): ");
//    for (int i = 0; i < 16; i++) {
//        printf("%d ", in_arr[i]);
//    }
//    uint8_t stat = reg_read8(STATUS_ADDR);
//    while (stat & 0x80) {
//        asm volatile("nop");
//        stat = reg_read8(STATUS_ADDR);
//    }
//    uint16_t test_out[32];
//    printf("Test Output (FP16 binary): ");
//    for (int i = 0; i < 4; i++) {
//        uint64_t current_out = reg_read64(OUTPUT_ADDR);
//        uint16_t* unpacked_out = (uint16_t*) &current_out;
//        for (int j = 0; j < 4; j++) {
//            test_out[i*4 + j] = unpacked_out[j];
//        }
//    }
//
//    uint16_t ref_out[32];
//    for (int i = 0; i < 16; i++) {
//        ref_out[i] = f16_from_int((int32_t) (in_arr[i] * 2));
//    }
//    printf("Reference Output (FP16 binary): ");
//    for (int i = 0; i < 16; i++) {
//        printf("%#x ", ref_out[i]);
//    }
//    printf("");
//
//    if (memcmp(test_out, ref_out, 16) == 0) {
//        printf("[TEST PASSED]: Test Output matches Reference Output.");
//    } else {
//        printf("[TEST FAILED]: Test Output does not match Reference Output.");
//    }
//    printf("");
//}

#define INPUT_ADDR      0x08800000
#define OUTPUT_ADDR     0x08800020
#define KERNEL_ADDR     0x08800040
#define START_ADDR      0x0880006C
#define LENGTH_ADDR     0x08800078
#define DILATION_ADDR   0x0880007C
#define INPUT_TYPE_ADDR 0x0880008E
#define RESET_ADDR      0x0880008E

void app_main() {

    printf("\rStarting test");
    reg_write8(RESET_ADDR, 1);

    // https://bwrcrepo.eecs.berkeley.edu/ee290c_ee194_intech22/sp24-chips/-/wikis/digital/dsp24/Programming-Interfaces#convolution-accelerator
    // reg_write8(INPUT_TYPE_ADDR, 0);
    int8_t in_arr[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint32_t in_len[1] = {16};
    uint16_t in_dilation[1] = {1};
    uint16_t in_kernel[8] = {0x4000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}; // {2, 0, 0, 0, 0, 0, 0, 0} in FP16              

    printf("\rSetting values of MMIO registers");
    set_conv_params(16, 1, ((uint64_t*) in_kernel));                  
    write_conv_dma(0, 16, in_arr);

    uint64_t cpu_start_cycles = READ_CSR("mcycle");
    printf("\rStarting Convolution");
    asm volatile ("fence");
    start_conv();

    uint64_t cpu_end_cycles = READ_CSR("mcycle");

    asm volatile ("fence");

    printf("\rWaiting for convolution to complete");
    
    printf("\rInput (INT8): ");
    for (int i = 0; i < 16; i++) {
        printf("%d ", in_arr[i]);
    }

    uint16_t test_out[32];
    read_conv_dma(0, 16, ((uint64_t*) test_out));
    printf("\rTest Output (FP16 binary): ");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            printf("0x%"PRIx64" ", test_out[i*4 + j]);
        }
    }

    uint16_t ref_out[32];
    for (int i = 0; i < 16; i++) {
        ref_out[i] = f16_from_int((int32_t) (in_arr[i] * 2));
    }
    printf("\rReference Output (FP16 binary): ");
    for (int i = 0; i < 16; i++) {
        printf("%#x ", ref_out[i]);
    }
    printf("\r");

    if (memcmp(test_out, ref_out, 16) == 0) {
        printf("\r[TEST PASSED]: Test Output matches Reference Output.");
        printf("\rmcycle: %llu", cpu_end_cycles - cpu_start_cycles);
    } else {
        printf("\r[TEST FAILED]: Test Output does not match Reference Output.");
        printf("\rmcycle: %llu", cpu_end_cycles - cpu_start_cycles);
    }
    printf("\r\r");

}

volatile uint8_t* i2s_config = (uint8_t*) 0x10042000;
volatile uint16_t* i2s_clkdiv = (uint16_t*) 0x10042014;
volatile uint64_t* i2s_lenqueue = (uint64_t*) 0x10042020;

void rando(void)
{
	size_t counter = 0;
	volatile size_t test;
	printf("Done uploading!\r\n");
	printf("Doing something else now!\r\n");
	*i2s_clkdiv = 7;
	printf("Set clock div!\r\n");
	*i2s_config = 243;
	printf("Done uploading, about to play!\r\n");

	for (counter = 0; counter < 32; counter++) {
		*i2s_lenqueue = audio[counter];
	}
	for (test = 0; test < 96; test++);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(int argc, char **argv) {
  /* MCU Configuration--------------------------------------------------------*/

  /* Configure the system clock */
  /* Configure the system clock */
  
  /* USER CODE BEGIN SysInit */
  UART_InitType UART_init_config;
  UART_init_config.baudrate = 115200;
  UART_init_config.mode = UART_MODE_TX_RX;
  UART_init_config.stopbits = UART_STOPBITS_2;
  uart_init(UART0, &UART_init_config);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */  
  /* USER CODE BEGIN Init */
  //app_init();
  /* USER CODE END Init */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  rando(); //compare_dma();
  //compare_conv();
  //simple_test();
  app_main();
  return 0;
  /* USER CODE END WHILE */
}

/*
 * Main function for secondary harts
 * 
 * Multi-threaded programs should provide their own implementation.
 */
void __attribute__((weak, noreturn)) __main(void) {
  while (1) {
   asm volatile ("wfi");
  }
}