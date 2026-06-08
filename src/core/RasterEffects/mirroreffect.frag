/*
#
# Friction - https://friction.graphics
#
# Copyright (c) Ole-André Rodlie and contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# See 'README.md' for more information.
#
*/

// Fork of enve - Copyright (C) 2016-2020 Maurycy Liebner

#version 330 core
layout(location = 0) out vec4 fragColor;

in vec2 texCoord;

uniform sampler2D tex;
uniform float centerX;
uniform float centerY;
uniform float angle;

void main(void) {
    // compute signed distance from pixel to the mirror axis
    vec2 center = vec2(centerX, centerY);
    vec2 p = texCoord;
    vec2 d = p - center;

    float cosA = cos(angle);
    float sinA = sin(angle);
    vec2 normal = vec2(-sinA, cosA);
    float dist = dot(d, normal);

    vec2 sampleCoord;
    if (dist >= 0.0) {
        sampleCoord = p;
    } else {
        // reflect across the axis: p' = p - 2 * dist * normal
        sampleCoord = p - 2.0 * dist * normal;
    }

    fragColor = texture(tex, sampleCoord);
}
