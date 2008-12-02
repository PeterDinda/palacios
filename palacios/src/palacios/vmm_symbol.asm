;  -*- fundamental -*-
;;
;; Symbol mangling macros
;; Copyright (c) 2001, David H. Hovemeyer <daveho@cs.umd.edu>
;;
;; Permission is hereby granted, free of charge, to any person
;; obtaining a copy of this software and associated documentation
;; files (the "Software"), to deal in the Software without restriction,
;; including without limitation the rights to use, copy, modify, merge,
;; publish, distribute, sublicense, and/or sell copies of the Software,
;; and to permit persons to whom the Software is furnished to do so,
;; subject to the following conditions:
;;
;; The above copyright notice and this permission notice shall be
;; included in all copies or substantial portions of the Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
;; ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
;; TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
;; PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
;; SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
;; FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
;; AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
;; FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
;; THE USE OR OTHER DEALINGS IN THE SOFTWARE.
;;



; This file defines macros for dealing with externally-visible
; symbols that must be mangled for some object file formats.
; For example, PECOFF requires a leading underscore, while
; ELF does not.

; EXPORT defines a symbol as global
; IMPORT references a symbol defined in another module

; Thanks to Christopher Giese for providing the NASM macros
; (thus saving me hours of frustration).

%ifndef __VMM_SYMBOL_ASM
%define __VMM_SYMBOL_ASM

%ifdef NEED_UNDERSCORE

%macro EXPORT 1
[GLOBAL _%1]
%define %1 _%1
%endmacro

%macro IMPORT 1
[EXTERN _%1]
%define %1 _%1
%endmacro

%else

%macro EXPORT 1
[GLOBAL %1]
%endmacro

%macro IMPORT 1
[EXTERN %1]
%endmacro

%endif

%endif
