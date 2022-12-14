/*
 * Copyright (c) 2016, 2022, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#include "precompiled.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "jfr/jni/jfrJavaSupport.hpp"
#include "jfr/jni/jfrUpcalls.hpp"
#include "jfr/support/jfrJdkJfrEvent.hpp"
#include "jvm_io.h"
#include "logging/log.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayKlass.hpp"
#include "oops/typeArrayOop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/os.hpp"
#include "utilities/exceptions.hpp"

static Symbol* jvm_upcalls_class_sym = NULL;
static Symbol* on_retransform_method_sym = NULL;
static Symbol* on_retransform_signature_sym = NULL;
static Symbol* bytes_for_eager_instrumentation_sym = NULL;
static Symbol* bytes_for_eager_instrumentation_sig_sym = NULL;
static Symbol* unhide_internal_types_sym = NULL;
static Symbol* unhide_internal_types_sig_sym = NULL;

static bool initialize(TRAPS) {
  static bool initialized = false;
  if (!initialized) {
    DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
    jvm_upcalls_class_sym = SymbolTable::new_permanent_symbol("jdk/jfr/internal/JVMUpcalls");
    on_retransform_method_sym = SymbolTable::new_permanent_symbol("onRetransform");
    on_retransform_signature_sym = SymbolTable::new_permanent_symbol("(JZZLjava/lang/Class;[B)[B");
    bytes_for_eager_instrumentation_sym = SymbolTable::new_permanent_symbol("bytesForEagerInstrumentation");
    bytes_for_eager_instrumentation_sig_sym = SymbolTable::new_permanent_symbol("(JZZLjava/lang/Class;[B)[B");
    unhide_internal_types_sym = SymbolTable::new_permanent_symbol("unhideInternalTypes");
    unhide_internal_types_sig_sym = SymbolTable::new_permanent_symbol("()V");
    initialized = unhide_internal_types_sig_sym != NULL;
  }
  return initialized;
}

static const typeArrayOop invoke(jlong trace_id,
                                 jboolean force_instrumentation,
                                 jboolean boot_class_loader,
                                 jclass class_being_redefined,
                                 jint class_data_len,
                                 const unsigned char* class_data,
                                 Symbol* method_sym,
                                 Symbol* signature_sym,
                                 jint& new_bytes_length,
                                 TRAPS) {
  DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
  const Klass* klass = SystemDictionary::resolve_or_fail(jvm_upcalls_class_sym, true, CHECK_NULL);
  assert(klass != NULL, "invariant");
  typeArrayOop old_byte_array = oopFactory::new_byteArray(class_data_len, CHECK_NULL);
  memcpy(old_byte_array->byte_at_addr(0), class_data, class_data_len);
  JavaValue result(T_OBJECT);
  JfrJavaArguments args(&result, klass, method_sym, signature_sym);
  args.push_long(trace_id);
  args.push_int(force_instrumentation);
  args.push_int(boot_class_loader);
  args.push_jobject(class_being_redefined);
  args.push_oop(old_byte_array);
  JfrJavaSupport::call_static(&args, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    ResourceMark rm(THREAD);
    log_error(jfr, system)("JfrUpcall failed for %s", method_sym->as_C_string());
    return NULL;
  }
  // The result should be a [B
  const oop res = result.get_oop();
  assert(res != NULL, "invariant");
  assert(res->is_typeArray(), "invariant");
  assert(TypeArrayKlass::cast(res->klass())->element_type() == T_BYTE, "invariant");
  const typeArrayOop new_byte_array = typeArrayOop(res);
  new_bytes_length = (jint)new_byte_array->length();
  return new_byte_array;
}

static const size_t ERROR_MSG_BUFFER_SIZE = 256;
static void log_error_and_throw_oom(jint new_bytes_length, TRAPS) {
  char error_buffer[ERROR_MSG_BUFFER_SIZE];
  jio_snprintf(error_buffer, ERROR_MSG_BUFFER_SIZE,
    "Thread local allocation (native) for " SIZE_FORMAT " bytes failed in JfrUpcalls", (size_t)new_bytes_length);
  log_error(jfr, system)("%s", error_buffer);
  JfrJavaSupport::throw_out_of_memory_error(error_buffer, CHECK);
}

void JfrUpcalls::on_retransform(jlong trace_id,
                                jclass class_being_redefined,
                                jint class_data_len,
                                const unsigned char* class_data,
                                jint* new_class_data_len,
                                unsigned char** new_class_data,
                                TRAPS) {
  DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
  assert(class_being_redefined != NULL, "invariant");
  assert(class_data != NULL, "invariant");
  assert(new_class_data_len != NULL, "invariant");
  assert(new_class_data != NULL, "invariant");
  if (!JdkJfrEvent::is_visible(class_being_redefined)) {
    return;
  }
  jint new_bytes_length = 0;
  initialize(THREAD);
  const typeArrayOop new_byte_array = invoke(trace_id,
                                             false,
                                             false, // not used
                                             class_being_redefined,
                                             class_data_len,
                                             class_data,
                                             on_retransform_method_sym,
                                             on_retransform_signature_sym,
                                             new_bytes_length,
                                             CHECK);
  assert(new_byte_array != NULL, "invariant");
  assert(new_bytes_length > 0, "invariant");
  // memory space must be malloced as mtInternal
  // as it will be deallocated by JVMTI routines
  unsigned char* const new_bytes = (unsigned char* const)os::malloc(new_bytes_length, mtInternal);
  if (new_bytes == NULL) {
    log_error_and_throw_oom(new_bytes_length, THREAD); // unwinds
  }
  assert(new_bytes != NULL, "invariant");
  memcpy(new_bytes, new_byte_array->byte_at_addr(0), (size_t)new_bytes_length);
  *new_class_data_len = new_bytes_length;
  *new_class_data = new_bytes;
}

void JfrUpcalls::new_bytes_eager_instrumentation(jlong trace_id,
                                                 jboolean force_instrumentation,
                                                 jboolean boot_class_loader,
                                                 jclass super,
                                                 jint class_data_len,
                                                 const unsigned char* class_data,
                                                 jint* new_class_data_len,
                                                 unsigned char** new_class_data,
                                                 TRAPS) {
  DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
  assert(super != NULL, "invariant");
  assert(class_data != NULL, "invariant");
  assert(new_class_data_len != NULL, "invariant");
  assert(new_class_data != NULL, "invariant");
  jint new_bytes_length = 0;
  initialize(THREAD);
  const typeArrayOop new_byte_array = invoke(trace_id,
                                             force_instrumentation,
                                             boot_class_loader,
                                             super,
                                             class_data_len,
                                             class_data,
                                             bytes_for_eager_instrumentation_sym,
                                             bytes_for_eager_instrumentation_sig_sym,
                                             new_bytes_length,
                                             CHECK);
  assert(new_byte_array != NULL, "invariant");
  assert(new_bytes_length > 0, "invariant");
  unsigned char* const new_bytes = NEW_RESOURCE_ARRAY_IN_THREAD_RETURN_NULL(THREAD, unsigned char, new_bytes_length);
  if (new_bytes == NULL) {
    log_error_and_throw_oom(new_bytes_length, THREAD); // this unwinds
  }
  assert(new_bytes != NULL, "invariant");
  memcpy(new_bytes, new_byte_array->byte_at_addr(0), (size_t)new_bytes_length);
  *new_class_data_len = new_bytes_length;
  *new_class_data = new_bytes;
}

bool JfrUpcalls::unhide_internal_types(TRAPS) {
  DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
  JavaValue result(T_VOID);
  const Klass* klass = SystemDictionary::resolve_or_fail(jvm_upcalls_class_sym, true, CHECK_false);
  assert(klass != NULL, "invariant");
  JfrJavaArguments args(&result, klass, unhide_internal_types_sym, unhide_internal_types_sig_sym);
  JfrJavaSupport::call_static(&args, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    CLEAR_PENDING_EXCEPTION;
    ResourceMark rm(THREAD);
    log_error(jfr, system)("JfrUpcall failed for %s", unhide_internal_types_sym->as_C_string());
    return false;
  }
  return true;
}
