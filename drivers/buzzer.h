/*!
 * \file buzzer.h
 * \brief Buzzer subsystem functions.
 * \details This file contains all the methods used for \
 * playing tones with buzzer.
 * The buzzer can play a number of different tones, represented as a
 * note array.
 * The buzzer output frequency can be calculated using:
 * \f[ \frac{T_{\mathrm{ACLK}}}{2*\mathrm{TA1CCR0}} \f]
 */
#ifndef BUZZER_H_
#define BUZZER_H_

/*!
 * \brief Note type.
 * \details This is a type representing a note. It is composed by:
 * - The first 4 MSB represent the pitch
 * - The next 2 bits represent the octave
 * - The following 10 bits are the duration in ms of the note.
 *
 * There are two "meta" notes:
 * - The note xxxF represents no tone (a rest).
 * - The note xxx0 represents the "stop note" marking the \
 *   end of a note sequence.
 *
 * \note The stop note is needed in the play loop to determine \
 * when to end the melody.
 */
typedef uint16_t note;

/*!
 * \brief Initialize buzzer subsystem.
 */
void buzzer_init(void);

/*!
 * \brief Play a sequence of notes using the buzzer.
 * \param notes An array of notes to play.
 */
void buzzer_play(note *notes);

/*!
 * \brief A pointer to the song the buzzer is currently playing.
 * \details If there is no song playing, the value will be null.
 */
note *buzzer_buffer;

/*!
 * \brief A counter to keep the index of the current playing note.
 */
uint8_t buzzer_counter;

/*!
 * \brief This is 1 if buzzer is finished playing.
 * \details This is used to signal the messagebus for buzzer finish events.
 */
uint8_t buzzer_finished;

/*!
 * \brief This macro returns true if the buzzer is actually playing a note.
 * \details This returns true if the buzzer is playing something and the note
 * it is currently playing is not a rest, in other words, when the buzzer does
 * not use SMLCK.
 */
#define BUZZER_PLAYING (buzzer_buffer != NULL && \
			! (*(buzzer_buffer + buzzer_counter) & 0xF))

#endif /*BUZZER_H_*/
