/*
 * Copyright (C) 2020 Aaron Tikuisis <Aaron.Tikuisis@uottawa.ca>
 * Copyright (C) 2020 Isaac Keslassy <isaac@ee.technion.ac.il>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * $Id: gtkscoremap.h,v 1.3 2022/10/22 10:47:46 plm Exp $
 */

#ifndef GTKSCOREMAP_H
#define GTKSCOREMAP_H

#include "gnubg-types.h"        /* for matchstate */

extern void
 GTKShowScoreMap(const matchstate ams[], int cube);

#endif                          /* GTKSCOREMAP_H */
