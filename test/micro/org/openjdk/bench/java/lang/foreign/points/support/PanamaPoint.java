/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package org.openjdk.bench.java.lang.foreign.points.support;

import java.lang.foreign.Arena;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;

import org.openjdk.bench.java.lang.foreign.CLayouts;

import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

import static java.lang.foreign.MemoryLayout.PathElement.groupElement;

public class PanamaPoint extends CLayouts implements AutoCloseable {

    public static final MemoryLayout LAYOUT = MemoryLayout.structLayout(
        C_INT.withName("x"),
        C_INT.withName("y")
    );

    private static final VarHandle VH_x = LAYOUT.varHandle(groupElement("x"));
    private static final VarHandle VH_y = LAYOUT.varHandle(groupElement("y"));
    private static final MethodHandle MH_distance;
    private static final MethodHandle MH_distance_ptrs;

    static {
        Linker abi = Linker.nativeLinker();
        System.loadLibrary("Point");
        SymbolLookup loaderLibs = SymbolLookup.loaderLookup();
        MH_distance = abi.downcallHandle(
                loaderLibs.find("distance").get(),
                FunctionDescriptor.of(C_DOUBLE, LAYOUT, LAYOUT)
        );
        MH_distance_ptrs = abi.downcallHandle(
                loaderLibs.find("distance_ptrs").get(),
                FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_POINTER)
        );
    }

    Arena arena;
    private final MemorySegment segment;

    public PanamaPoint(int x, int y) {
        this.arena = Arena.openConfined();
        this.segment = MemorySegment.allocateNative(LAYOUT, arena.scope());
        setX(x);
        setY(y);
    }

    public void setX(int x) {
        VH_x.set(segment, x);
    }

    public int getX() {
        return (int) VH_x.get(segment);
    }

    public void setY(int y) {
        VH_y.set(segment, y);
    }

    public int getY() {
        return (int) VH_y.get(segment);
    }

    public double distanceTo(PanamaPoint other) {
        try {
            return (double) MH_distance.invokeExact(segment, other.segment);
        } catch (Throwable throwable) {
            throw new InternalError(throwable);
        }
    }

    public double distanceToPtrs(PanamaPoint other) {
        try {
            return (double) MH_distance_ptrs.invokeExact(segment, other.segment);
        } catch (Throwable throwable) {
            throw new InternalError(throwable);
        }
    }

    @Override
    public void close() {
        arena.close();
    }
}
