#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define ADS1115_IOCTL_MAGIC 'a'
#define ADS1115_IOCTL_READ_AIN0 _IOR(ADS1115_IOCTL_MAGIC, 1, int)
#define ADS1115_IOCTL_READ_AIN1 _IOR(ADS1115_IOCTL_MAGIC, 2, int)
#define ADS1115_IOCTL_READ_AIN2 _IOR(ADS1115_IOCTL_MAGIC, 3, int)
#define ADS1115_IOCTL_READ_AIN3 _IOR(ADS1115_IOCTL_MAGIC, 4, int)

#define V_REF 4.868        
#define MAX_ADC_VALUE 32767

int main() {
    int fd = open("/dev/ads1115", O_RDWR);
    while (1) {
        
        if (fd < 0) {
            perror("Failed to open /dev/ads1115");
            return 1;
        }

        int raw[4];
        float volt[4];

        unsigned long cmds[4] = {
            ADS1115_IOCTL_READ_AIN0,
            ADS1115_IOCTL_READ_AIN1,
            ADS1115_IOCTL_READ_AIN2,
            ADS1115_IOCTL_READ_AIN3
        };

        for (int i = 0; i < 4; ++i) {
            if (ioctl(fd, cmds[i], &raw[i]) < 0) {
                perror("IOCTL failed");
                close(fd);
                return 1;
            }

            if (raw[i] < 0) raw[i] = 0;

            volt[i] = ((float)raw[i] / MAX_ADC_VALUE) * V_REF;
        }

        printf("* A0 * ADC Value: %5d || Voltage value: %.3f V\n", raw[0], volt[0]);
        printf("* A1 * ADC Value: %5d || Voltage value: %.3f V\n", raw[1], volt[1]);
        printf("* A2 * ADC Value: %5d || Voltage value: %.3f V\n", raw[2], volt[2]);
        printf("* A3 * ADC Value: %5d || Voltage value: %.3f V\n", raw[3], volt[3]);
        printf("----------------------------------\n");

        
    }

    return 0;
}
