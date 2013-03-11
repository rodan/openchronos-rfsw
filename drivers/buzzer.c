/*
    drivers/timer.c: Openchronos TA0 timer driver

    Copyright (C) 2012 Aljaž Srebrnič <a2piratesoft@gmail.com>

	http://www.openchronos-ng.sourceforge.net

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <openchronos.h>
#include <stdlib.h>
#include <string.h>


#include "buzzer.h"
#include "timer.h"

#define DURATION(note) (note >> 6)
#define OCTAVE(note) ((note >> 4) & 0x0003)
#define PITCH(note) (note & 0x000F)

uint16_t base_notes[13] = {
	2383, /* 1: A  */
	2249, /* 2: A# */
	2123, /* 3: B  */
	2003, /* 4: C  */
	1891, /* 5: C# */
	1785, /* 6: D  */
	1685, /* 7: D# */
	1590, /* 8: E  */
	1501, /* 9: F  */
	1417, /* A: F# */
	1337, /* B: G  */
	1262  /* C: G# */
};

inline void buzzer_init(void)
{
	/* Reset TA1R, TA1 runs from 32768Hz ACLK */
	TA1CTL = TACLR | TASSEL__SMCLK | MC__STOP;

	/* Enable IRQ, set output mode "toggle" */
	TA1CCTL0 = OUTMOD_4;

	/* initialize buzzer_buffer value */
	buzzer_finished = 0;

	/* Play "welcome" chord: A major */
	note welcome[4] = {0x1901, 0x1904, 0x1908, 0x0000};
	buzzer_play(welcome);
}

void buzzer_stop(void)
{
	/* Stop PWM timer */
	TA1CTL &= ~MC_3;

	/* Disable buzzer PWM output */
	P2OUT &= ~BIT7;
	P2SEL &= ~BIT7;

	/* Clear PWM timer interrupt */
	TA1CCTL0 &= ~CCIE;

	/* Signal messagebus that we're finished */
	buzzer_finished = 1;

	/* Buzzer buffer must be freed by the application using it */
	free(buzzer_buffer);
	buzzer_buffer = NULL;
	buzzer_counter = 0;
	/* This is not actually needed, but I prefer
	 * having the counter equal to 0 if we're not
	 * playing anything.
	 */
}

void buzzer_callback()
{
	/* Start by incrementing the counter; we are playing the next note
	 * This is here because the index must atually point to the note
	 * currently playing, so main knows if we can go to LPM3 */
	buzzer_counter++;

	/* Here the -1 is needed for the offset of buzzer_counter due to the
	 * increment above. */
	note n = *(buzzer_buffer + buzzer_counter - 1);
	/* 0x000F is the "stop bit" */
	if(PITCH(n) == 0) {
		/* Stop buzzer */
		buzzer_stop();
		return;
	}
	if (PITCH(n) == 0x000F) {
		/* Stop the timer! We are playing a rest */
		TA1CTL &= ~MC_3;
	} else {
		/* Set PWM frequency */
		TA1CCR0 = base_notes[PITCH(n)] >> OCTAVE(n);

		/* Start the timer */
		TA1CTL |= MC__UP;
	}

	/* Delay for DURATION(*n) milliseconds, */
	timer0_delay_callback(DURATION(n), &buzzer_callback);
}

void buzzer_play(note *notes)
{
	/* TODO: Define correct behaviour here. Should we error out or just
	 * return? Should we return an error code? or just crash, to identify
	 * and eliminate any race condition? Or just replace the buffer? */
	if(buzzer_buffer != NULL)
		buzzer_stop();

	uint8_t len = 0;
	while (notes[len] != 0) len++;

	len++; /* We count the end note to get actual length */
	buzzer_buffer = malloc(len * sizeof(notes));
	memcpy(buzzer_buffer, notes, len * sizeof(notes));
	buzzer_counter = 0;

	/* Allow buzzer PWM output on P2.7 */
	P2SEL |= BIT7;

	/* Play first note */
	buzzer_callback();
}
