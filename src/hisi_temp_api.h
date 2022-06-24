
#ifndef __HI_TEMP_API_H__
#define __HI_TEMP_API_H__


#include <linux/types.h>




extern int hitemp_init(void);
extern void hitemp_deinit(void);
extern int hisi_get_temp(s32 *temp);



#endif
