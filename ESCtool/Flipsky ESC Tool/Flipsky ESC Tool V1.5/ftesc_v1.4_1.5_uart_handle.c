#include <math.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  UART_OBTAIN_DATA_ONCE = 0,  // 获取当前数据
  UART_CONTROL_AND_OBTAIN_DATA_ONCE = 2,
  UART_SET_DUTY = 3,          // 设置占空比
  UART_SET_CURRENT = 4,       // 设置电流
  UART_SET_CURRENT_GEAR = 5,  // 设置带有档位的电流
  UART_SET_BRAKE_CURRENT = 6, // 设置刹车电流
  UART_CAN_FWARD = 16,
  UART_OBTAIN_FIRMWARE_VERSION = 17, // 软件版本
  UART_KEEP_LIVE = 25,
  UART_SET_AUTO_OBTAIN_REALTIME_DATA = 26,
  UART_RESET_AND_REBOOT_FTESC = 29,
  UART_OBTAIN_ALL_FTESC_ID = 30,
  UART_SET_CURRENT_GEAR_AND_OBTAIN_DATA = 32, // 设置电流
  UART_REBOOT_FTESC = 35,
  UART_SET_ID_CURRENT = 37,   // 设置电流
  UART_SET_POSITION = 38,
  UART_SET_SPEED = 39,
} UART_COMMAND;

static const unsigned char aucCRCHi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40};

static const unsigned char aucCRCLo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
    0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
    0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
    0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
    0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
    0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
    0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40};

typedef struct
{
  unsigned char controller_id;
  unsigned char fault;
  float inpVoltage;
  float inputCurrent;
  float motorCurrent;
  float rpm;
  float dutyCycleNow;
  float temp_fet;
  float temp_motor;
  float cpu_load;
  float encoder_angle;
} ftesc_realtime_data;

unsigned short crc16_ftesc(unsigned char *buf, unsigned int len)
{
  unsigned char ucCRCHi = 0xFF;
  unsigned char ucCRCLo = 0xFF;
  int iIndex;
  while (len--)
  {
    iIndex = ucCRCLo ^ *(buf++);
    ucCRCLo = (unsigned char)(ucCRCHi ^ aucCRCHi[iIndex]);
    ucCRCHi = aucCRCLo[iIndex];
  }
  return (unsigned short)(ucCRCHi << 8 | ucCRCLo);
}

void valueDecompose_I16(uint8_t *dataNum, int16_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_U16(uint8_t *dataNum, uint16_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_I24(uint8_t *dataNum, int32_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 16;
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_U24(uint8_t *dataNum, uint32_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 16;
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_I32(uint8_t *dataNum, int32_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 24;
  dataNum[(*numInd)++] = value >> 16;
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_U32(uint8_t *dataNum, uint32_t value, int32_t *numInd)
{
  dataNum[(*numInd)++] = value >> 24;
  dataNum[(*numInd)++] = value >> 16;
  dataNum[(*numInd)++] = value >> 8;
  dataNum[(*numInd)++] = value;
}

void valueDecompose_F16(uint8_t *dataNum, float value, float multiple, int32_t *numInd)
{
  valueDecompose_I16(dataNum, (int16_t)(value * multiple), numInd);
}

void valueDecompose_F32(uint8_t *dataNum, float value, float multiple, int32_t *numInd)
{
  valueDecompose_I32(dataNum, (int32_t)(value * multiple), numInd);
}

void valueDecompose_AF32(uint8_t *dataNum, float value, int32_t *numInd)
{
  int exp = 0;
  float frexpf_t = frexpf(value, &exp);
  float frexpf_abs = fabsf(frexpf_t);
  uint32_t frexpf_uint = 0;

  if (frexpf_abs >= 0.5f)
  {
    frexpf_uint = (uint32_t)((frexpf_abs - 0.5f) * 2.0f * 8388608.0f);
    exp += 126;
  }

  uint32_t retval = ((exp & 0xFF) << 23) | (frexpf_uint & 0x7FFFFF);
  if (frexpf_t < 0)
  {
    retval |= 1U << 31;
  }

  valueDecompose_U32(dataNum, retval, numInd);
}

int16_t valueCompose_I16(const uint8_t *dataNum, int32_t *numInd)
{
  int16_t retval = ((uint16_t)dataNum[*numInd]) << 8 |
                   ((uint16_t)dataNum[*numInd + 1]);
  *numInd += 2;
  return retval;
}

uint16_t valueCompose_U16(const uint8_t *dataNum, int32_t *numInd)
{
  uint16_t retval = ((uint16_t)dataNum[*numInd]) << 8 |
                    ((uint16_t)dataNum[*numInd + 1]);
  *numInd += 2;
  return retval;
}

int32_t valueCompose_I24(const uint8_t *dataNum, int32_t *numInd)
{
  int32_t retval = ((uint32_t)dataNum[*numInd]) << 16 |
                   ((uint32_t)dataNum[*numInd + 1]) << 8 |
                   ((uint32_t)dataNum[*numInd + 2]);
  *numInd += 3;
  return retval;
}

uint32_t valueCompose_U24(const uint8_t *dataNum, int32_t *numInd)
{
  uint32_t retval = ((uint32_t)dataNum[*numInd]) << 16 |
                    ((uint32_t)dataNum[*numInd + 1]) << 8 |
                    ((uint32_t)dataNum[*numInd + 2]);
  *numInd += 3;
  return retval;
}

int32_t valueCompose_I32(const uint8_t *dataNum, int32_t *numInd)
{
  int32_t retval = ((uint32_t)dataNum[*numInd]) << 24 |
                   ((uint32_t)dataNum[*numInd + 1]) << 16 |
                   ((uint32_t)dataNum[*numInd + 2]) << 8 |
                   ((uint32_t)dataNum[*numInd + 3]);
  *numInd += 4;
  return retval;
}

uint32_t valueCompose_U32(const uint8_t *dataNum, int32_t *numInd)
{
  uint32_t retval = ((uint32_t)dataNum[*numInd]) << 24 |
                    ((uint32_t)dataNum[*numInd + 1]) << 16 |
                    ((uint32_t)dataNum[*numInd + 2]) << 8 |
                    ((uint32_t)dataNum[*numInd + 3]);
  *numInd += 4;
  return retval;
}

float valueCompose_F16(const uint8_t *dataNum, float multiple, int32_t *numInd)
{
  return (float)valueCompose_I16(dataNum, numInd) / multiple;
}

float valueCompose_F32(const uint8_t *dataNum, float multiple, int32_t *numInd)
{
  return (float)valueCompose_I32(dataNum, numInd) / multiple;
}

float valueCompose_AF32(const uint8_t *dataNum, int32_t *numInd)
{
  uint32_t retval = valueCompose_U32(dataNum, numInd);

  int exp = (retval >> 23) & 0xFF;
  uint32_t frexpf_uint = retval & 0x7FFFFF;
  bool negative = retval & (1U << 31);

  float frexpf_t = 0.0;
  if (exp != 0 || frexpf_uint != 0)
  {
    frexpf_t = (float)frexpf_uint / (8388608.0f * 2.0f) + 0.5f;
    exp -= 126;
  }

  if (negative)
  {
    frexpf_t = -frexpf_t;
  }

  return ldexpf(frexpf_t, exp);
}

void USART1_Sendstring(uint8_t *str, uint8_t LEN)
{
  uint8_t i;
  for (i = 0; i < LEN; i++)
  {
    USART1->TDR = *str;
    while ((USART1->ISR & 0x40) == 0);
    str++;
  }
}

void FTESC_Uart_packSendPayload(uint8_t *payload, int Dlen)
{
  uint8_t messageSend[512];
  uint16_t crcPayload;
  int count = 0;

  crcPayload = crc16_ftesc(payload, Dlen);
  if (Dlen <= 255)
  {
    messageSend[count++] = 0xAA;
    messageSend[count++] = Dlen;
  }
  else if (Dlen <= 65535)
  {
    messageSend[count++] = 0xBB;
    messageSend[count++] = (uint8_t)(Dlen >> 8);
    messageSend[count++] = (uint8_t)(Dlen & 0XFF);
  }
  memcpy(&messageSend[count], payload, Dlen);
  count += Dlen;

  messageSend[count++] = (uint8_t)(crcPayload >> 8);
  messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
  messageSend[count++] = 0xDD;
  messageSend[count] = '\0';
  USART1_Sendstring(messageSend, count);
}

// 执行该函数, 发送指令到MCU端
void Ftesc_Obtain_Realtime_Data(void)
{
  uint8_t payload[1];
  payload[0] = UART_OBTAIN_DATA_ONCE;
  FTESC_Uart_packSendPayload(payload, 1); // 串口发送函数
}

ftesc_realtime_data esc_data;

// 一般使用空闲中断触发串口接收完成，缓冲区uart_rx_buff通过DMA传输。
// 空闲中断触发后，即可以对缓冲区里的数据进行解析
int8_t Ftesc_Uart_processReadPacket(uint8_t *uart_rx_buff)
{
  uint16_t data_len = 0;
  uint8_t *data = 0;
  // 判断帧头是否正确
  if (uart_rx_buff[0] == 0xAA)
  {
    data_len = uart_rx_buff[1];
    data = uart_rx_buff + 2;
  }
  else if (uart_rx_buff[0] == 0xBB)
  {
    data_len |= (uint16_t)uart_rx_buff[1] << 8; // 高8位
    data_len |= (uint16_t)uart_rx_buff[2];      // 低8位
    data = uart_rx_buff + 3;
  }
  else
  {
    return -1; // 数据包格式异常，必须是以0XAA或者0XBB开头
  }

  // 计算链路层数据crc16校验码以及获取接收到的crc16校验码
  uint16_t data_crc16_calc = crc16_ftesc(data, data_len);
  uint16_t data_crc16_rece = (uint16_t)data[data_len] << 8 | (uint16_t)data[data_len + 1];

  // 判断CRC16校验是否正确
  if (data_crc16_calc != data_crc16_rece)
  {
    return -2; // 数据包CRC16校验失败
  }

  // 判断帧尾是否正确
  if (data[data_len + 2] != 0XDD)
  { // 必须是以0XDD结尾
    return -3;
  }

  UART_COMMAND uart_cmd = (UART_COMMAND)data[0]; // 获取指令
  uint16_t index = 0;
  data++; // 偏移至DATA区域
  switch (uart_cmd)
  {
  case UART_OBTAIN_DATA_ONCE:
  {
    esc_data.controller_id = data[index++];
    esc_data.fault = data[index++];
    esc_data.inpVoltage = valueCompose_F16(data, 100.0f, &index);
    esc_data.inputCurrent = valueCompose_F32(data, 1000000.0f, &index);
    esc_data.motorCurrent = valueCompose_F32(data, 1000000.0f, &index);
    esc_data.rpm = valueCompose_F32(data, 1.0f, &index);
    esc_data.dutyCycleNow = valueCompose_F16(data, 10000.0f, &index);
    esc_data.temp_fet = valueCompose_F16(data, 100.0f, &index);
    esc_data.temp_motor = valueCompose_F16(data, 100.0f, &index);
    esc_data.cpu_load = valueCompose_F16(data, 10000.0f, &index);
    esc_data.encoder_angle = valueCompose_F32(data, 1000000.0f, &index);
  }
  break;
  default:
    break;
  }
  return 0; // 帧解析完成
}
