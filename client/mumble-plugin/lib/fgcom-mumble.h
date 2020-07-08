/* 
 * This file is part of the FGCom-mumble distribution (https://github.com/hbeni/fgcom-mumble).
 * Copyright (c) 2020 Benedikt Hallinger
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Define opwn plugin api
//

#ifndef FGCOM_MUMBLE_H
#define FGCOM_MUMBLE_H


/*
 * Is the plugin currently active?
 * 
 * @return bool true if yes
 */
bool fgcom_isPluginActive();

/*
 * Handle PTT change of local user
 * 
 * This will check the local radio state and activate the mic if all is operable.
 * When no PTT or no radio is operable, mic is closed.
 */
void fgcom_handlePTT();


#endif