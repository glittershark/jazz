#!/usr/bin/env python3

boofer_size = 200000
max_fade_time = 10000
# list of lists
# first element of each inner list is a float
# following elements are lists of 4 floats:
#  - type of update: 0 is erase, 1 is write
#  - clock time at which update is fully written
#  -
boofer = [[0.0] for _ in range(boofer_size)]
indices_to_update = [
    [] for _ in range(max_fade_time)
]  # positions of update that are fully faded in at this time, indexed by buffer position
global_clock = 0  # counts up, 1 per sample
global_clock_max = 300000


def erase(index, clock_time, value, samples=max_fade_time):
    # ADD UPDATE TO BOOFER, THEN SAY WHAT CLOCK TIME TO MAKE THE UPDATE
    boofer[index].append(
        [
            0,  # erase
            clock_time + samples,  # when is this update done
            value,  # how much to change by
            samples,
        ]
    )
    if index not in indices_to_update[clock_time % max_fade_time]:
        indices_to_update[clock_time % max_fade_time].append(index)


def write(index, clock_time, value, samples=max_fade_time):
    # ADD UPDATE TO BOOFER, THEN SAY WHAT CLOCK TIME TO MAKE THE UPDATE
    boofer[index].append([1, clock_time + samples, value, value / samples])
    if index not in indices_to_update[clock_time % max_fade_time]:
        indices_to_update[clock_time % max_fade_time].append(index)


def pre_housekeeping(clock_time):
    # WE KNOW WHAT INDICES IN BOOFER TO UPDATE FOR ANY GIVEN CLOCK SO WE DO THAT
    for index in indices_to_update[clock_time % max_fade_time]:
        update(index, clock_time)
    indices_to_update[clock_time % max_fade_time].clear()


def update(index, clock_time):

    global global_clock_max

    ## WE ARE RESOLVING ALL RIPE UPDATES INTO MAIN BOOFER VAL
    content = boofer[index]
    ripe_updates = []

    for i in range(1, len(content)):
        update = content[i]
        if not update[0]:  ##THIS MEANS ITS AN ERASE
            time_til_ripe = (update[1] - clock_time) % global_clock_max
            if time_til_ripe <= 0:
                ripe_updates.append(i)
                content[0] *= update[2]

    ## ERASE BEFORE WRITE JUST FOR CONSISTENT BEHAVIOR

    for i in range(1, len(content)):
        update = content[i]
        if update[0]:  ##THIS MEANS ITS A WRITE
            time_til_ripe = (update[1] - clock_time) % global_clock_max
            if time_til_ripe <= 0:
                ripe_updates.append(i)
                content[0] += update[2]

    boofer[index] = [content[i] for i in range(len(content)) if i not in ripe_updates]

    # boofer[index] = [_[i] for i in range(len(content)) if i not in ripe_updates]


def read(index, clock_time):

    global global_clock_max
    ## DYNAMICALLY READ FADING IN ERASES AND WRITES
    content = boofer[index]
    value = content[0]  ## THIS IS NOT IN PLACE

    for i in range(1, len(content)):
        update = content[i]
        if not update[0]:  ## ERASE
            time_til_ripe = (update[1] - clock_time) % global_clock_max
            frac_offset = (update[3] * update[2]) / (1 - update[2])
            factor = (frac_offset) / ((update[3] - time_til_ripe) + frac_offset + 1)
            value *= factor

    ## ERASE BEFORE WRITE JUST FOR CONSISTENT BEHAVIOR

    for i in range(1, len(content)):
        update = content[i]
        if update[0]:  ## WRITE
            time_til_ripe = (update[1] - clock_time) % global_clock_max
            target_val = update[2]
            fading_in_val = target_val - (time_til_ripe * update[3])
            value += fading_in_val
    return value


def full_cycle(heads):
    global global_clock_max
    global global_clock

    pre_housekeeping(global_clock)

    wet_signal = 0

    for head in heads:
        if head[0] == -1:  # read
            wet_signal += read(head[1], global_clock)
        if head[0] == 0:  # erase
            erase(head[1], global_clock, head[2])
        if head[0] == 1:  # write
            write(head[1], global_clock, head[2])

    global_clock = (global_clock + 1) % global_clock_max
