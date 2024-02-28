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

#include "IO.h"
#include "stm32h7xx_hal.h"

// Configure as output
IO::IO(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin) :
		_GPIO(GPIOx), _pin(GPIO_Pin), _isInput(false), _pull() {
	ConfigurePin(GPIOx, GPIO_Pin, false, false, PULL_NONE);
}

// Configure as input
IO::IO(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin, pull_t pull) :
		_GPIO(GPIOx), _pin(GPIO_Pin), _isInput(true), _pull() {
	ConfigurePin(GPIOx, GPIO_Pin, true, false, pull);
}

IO::~IO() {
	if (!_GPIO)
		return;
	HAL_GPIO_DeInit(_GPIO, _pin);

	// Calculate pin index by extracting bit index from GPIO_PIN
	uint16_t pinIndex;
	uint16_t tmp = _pin;
	for (pinIndex = -1; tmp != 0; pinIndex++)
		tmp = tmp >> 1;
}

void IO::ConfigurePin(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin, bool isInput,
		bool isOpenDrain, pull_t pull) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	// GPIO Ports Clock Enable
	if (GPIOx == GPIOA)
		__HAL_RCC_GPIOA_CLK_ENABLE();
	else if (GPIOx == GPIOB)
		__HAL_RCC_GPIOB_CLK_ENABLE();
	else if (GPIOx == GPIOC)
		__HAL_RCC_GPIOC_CLK_ENABLE();
	else if (GPIOx == GPIOD)
		__HAL_RCC_GPIOD_CLK_ENABLE();
	else if (GPIOx == GPIOE)
		__HAL_RCC_GPIOE_CLK_ENABLE();
	else if (GPIOx == GPIOF)
		__HAL_RCC_GPIOF_CLK_ENABLE();
	else if (GPIOx == GPIOG)
		__HAL_RCC_GPIOG_CLK_ENABLE();
	else {
		_GPIO = 0;
		return;
	}

	// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET);

	// Configure pin as output or input
	_isInput = isInput;
	_isOpenDrain = isOpenDrain;
	if (isInput)
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	else if (isOpenDrain)
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	else
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;

	_pull = pull;
	if (pull == PULL_NONE)
		GPIO_InitStruct.Pull = GPIO_NOPULL;
	else if (pull == PULL_UP)
		GPIO_InitStruct.Pull = GPIO_PULLUP;
	else if (pull == PULL_DOWN)
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;

	// Configure GPIO
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void IO::ChangeToInput(pull_t pull) {
	ConfigurePin(_GPIO, _pin, true, false, _pull);
}

void IO::ChangeToOutput(bool state) {
	ConfigurePin(_GPIO, _pin, false, false, PULL_NONE);
	Set(state);
}

void IO::ChangeToOpenDrain(bool state) {
	ConfigurePin(_GPIO, _pin, false, true, PULL_NONE);
	Set(state);
}

void IO::RegisterInterrupt(interrupt_trigger_t trigger,
		SemaphoreHandle_t semaphore) {
	if (!_GPIO || !_isInput)
		return;

	_InterruptSemaphore = semaphore;
	ConfigureInterrupt(trigger);
}

void IO::Set(bool state) {
	if (!_GPIO || _isInput)
		return;
	if (state)
		_GPIO->BSRRL = _pin;
	else
		_GPIO->BSRRH = _pin;
}

void IO::High() {
	if (!_GPIO || _isInput)
		return;
	_GPIO->BSRRL = _pin;
}

void IO::Low() {
	if (!_GPIO || _isInput)
		return;
	_GPIO->BSRRH = _pin;
}

void IO::Toggle() {
	if (!_GPIO || _isInput)
		return;
	_GPIO->ODR ^= _pin;
}

bool IO::Read() {
	if (!_GPIO || !_isInput)
		return false;
	if ((_GPIO->IDR & _pin) != (uint32_t) GPIO_PIN_RESET)
		return true;
	else
		return false;
}
