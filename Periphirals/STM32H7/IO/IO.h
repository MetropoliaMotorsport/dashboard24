/* Copyright (C) 2018-2019 Thomas Jespersen, TKJ Electronics. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the MIT License
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the MIT License for further details.
 *
 * Contact information
 * ------------------------------------------
 * Thomas Jespersen, TKJ Electronics
 * Web      :  http://www.tkjelectronics.dk
 * e-mail   :  thomasj@tkjelectronics.dk
 * ------------------------------------------
 */

#ifndef PERIPHIRALS_IO_H
#define PERIPHIRALS_IO_H

#include "stm32h7xx_hal.h"

class IO
{
public:
	typedef enum interrupt_trigger_t {
		TRIGGER_RISING = 0,
		TRIGGER_FALLING,
		TRIGGER_BOTH
	}interrupt_trigger_t;

	typedef enum pull_t {
		PULL_NONE = 0,
		PULL_UP,
		PULL_DOWN,
	}pull_t;

public:
	IO(GPIO_TypeDef * GPIOx, uint32_t GPIO_Pin); // configure as output
	IO(GPIO_TypeDef * GPIOx, uint32_t GPIO_Pin, pull_t pull);// configure as input
	~IO();

	void Set(bool state);
	bool Read();
	void High();
	void Low();
	void Toggle();

	void ChangeToInput(pull_t pull);
	void ChangeToOutput(bool state = false);
	void ChangeToOpenDrain(bool state = false);

private:
	void ConfigurePin(GPIO_TypeDef * GPIOx, uint32_t GPIO_Pin, bool isInput, bool isOpenDrain, pull_t pull);

private:
	GPIO_TypeDef * _GPIO;
	uint32_t _pin;
	bool _isInput;
	bool _isOpenDrain;
	pull_t _pull;
};

#endif
