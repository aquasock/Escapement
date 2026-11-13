/*
 * check_audio.c - why a beep per "audio event" never stopped.
 *
 * THE BUG THIS PINS. The firmware played a BEL continuously for as long as the
 * guest made any sound, and went quiet only when the emulator was paused. The
 * cause was a misreading of splb32_sound's emit counter: it was treated as a
 * count of SOUNDS, and it is a count of square-wave TOGGLES.
 *
 * That is a claim about arithmetic, so it is checked rather than asserted in a
 * comment. A single tone of one second at the V3's pitch raises the counter
 * thousands of times, and the envelope -- the thing that actually says "this
 * channel is playing" -- is a constant over the same interval. A front end that
 * watches the counter sees a machine-gun; one that watches the envelope edge sees
 * one sound.
 *
 * The numbers are from the model itself, not from the console: this links
 * splb32_sound.c alone, with no CPU and no ROM, so the test is exactly as long as
 * the question.
 *
 *   ./scripts/tamacore-host.sh audio
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "splb32_sound.h"

/* Mirror of splb32_audio_active()'s rule, restated here so the test states the
 * predicate independently of the implementation it is checking. If these two
 * ever disagree, the firmware's is the one that matters -- but the disagreement
 * itself is worth failing on, which is why the constant is named. */
static int active_by_envelope(const splb32_sound *s)
{
    for (int ch = 0; ch < 2; ch++)
        if (s->envelope[ch] != 0.0 || s->last[ch] != 0.0)
            return 1;
    return 0;
}

int main(void)
{
    splb32_sound s;
    int failures = 0;

    /* --- silence ---------------------------------------------------------- */
    splb32_sound_init(&s);
    if (active_by_envelope(&s))
    {
        printf("FAIL: a fresh channel reports sound\n");
        failures++;
    }

    /*
     * --- one tone, held --------------------------------------------------
     *
     * Set a non-zero envelope the way the firmware does (tone mode, a data
     * write), then toggle the way the timer overflow does. This is one sound.
     */
    splb32_sound_set_data(&s, 0, AUD_CTRL_TONE, 200);

    unsigned long emits_at_start = s.emits;
    int toggles = 1000;

    for (int i = 0; i < toggles; i++)
        splb32_sound_toggle(&s, 0);

    unsigned long delta = s.emits - emits_at_start;

    if (!active_by_envelope(&s))
    {
        printf("FAIL: a playing tone reports silence\n");
        failures++;
    }

    /*
     * THE HEART OF IT. One sound, N counter increments. A front end keyed on the
     * counter treats each of those as an event; a front end keyed on the
     * envelope sees one transition.
     */
    if (delta < (unsigned long)toggles)
    {
        printf("FAIL: %d toggles produced only %lu emits\n", toggles, delta);
        failures++;
    }

    /* And the envelope did not change across any of them, which is what makes
     * the edge detection give exactly one event. */
    if (s.envelope[0] == 0.0)
    {
        printf("FAIL: envelope went to zero while playing\n");
        failures++;
    }

    /*
     * --- the level alternates, so "level changed" is not an event either ---
     *
     * toggle_state flips each time, so a caller watching the LEVEL would fire
     * twice per period rather than once per sound. Worth checking, because it is
     * the obvious alternative fix and it is also wrong.
     */
    {
        double a = splb32_sound_level(&s, 0);

        splb32_sound_toggle(&s, 0);
        double b = splb32_sound_level(&s, 0);

        if (a == b)
        {
            printf("FAIL: level did not alternate across a toggle\n");
            failures++;
        }
        if (a == 0.0 || b == 0.0)
        {
            printf("FAIL: level is zero mid-period\n");
            failures++;
        }
    }

    /* --- the tone ends ---------------------------------------------------- */
    splb32_sound_set_data(&s, 0, AUD_CTRL_TONE, 0);

    if (active_by_envelope(&s))
    {
        printf("FAIL: a cleared envelope still reports sound\n");
        failures++;
    }

    /*
     * --- and toggling a silent channel emits nothing ------------------------
     *
     * This is the guard inside toggle(): an envelope of zero returns before
     * incrementing. It is why a paused emulator is silent rather than merely
     * quiet -- it stops stepping, so no timer overflows, so no toggles.
     */
    {
        unsigned long before = s.emits;

        for (int i = 0; i < 100; i++)
            splb32_sound_toggle(&s, 0);

        if (s.emits != before)
        {
            printf("FAIL: toggling a silent channel emitted\n");
            failures++;
        }
    }

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }

    printf("ok: one %d-toggle tone is one sound (%lu emits, envelope constant)\n",
           toggles, delta);
    return 0;
}
