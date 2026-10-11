/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_threads.h"

void PG_lineart_spin_init(SpinLock *spin);
void PG_lineart_spin_lock(SpinLock *spin);
void PG_lineart_spin_unlock(SpinLock *spin);
void PG_lineart_spin_end(SpinLock *spin);

#define BLI_spin_init PG_lineart_spin_init
#define BLI_spin_lock PG_lineart_spin_lock
#define BLI_spin_unlock PG_lineart_spin_unlock
#define BLI_spin_end PG_lineart_spin_end
