/*
 * stomptrix_msgs.h
 *
 *  Created on: Jul 2, 2017
 *      Author: Mark
 */

#ifndef SRC_STOMPTRIX_MSGS_H_
#define SRC_STOMPTRIX_MSGS_H_


#pragma pack(push, 1)

typedef struct{
	uint8_t msg_id;
	uint8_t data_len;
	uint8_t data[1u];

}stomptrix_ctrl_msg_t;

#define LED_SET_STATIC_COLOUR 0x01u
typedef struct{
	uint32_t colour;
}led_set_static_colour_t;

#define LED_SET_SLIDE_ANIMATION 0x02u
typedef struct{
	uint32_t colour;
	uint8_t single_or_cont;
}led_slide_animation_t;

#pragma pack(pop)


#endif /* SRC_STOMPTRIX_MSGS_H_ */
