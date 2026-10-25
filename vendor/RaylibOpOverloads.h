#ifndef RAYLIB_OP_OVERLOADS_HPP_INCLUDED
#define RAYLIB_OP_OVERLOADS_HPP_INCLUDED
#include <raylib.h>
#include <raymath.h>

// Note: (Gabe) I dont want this so disabling it, and it seems to be fine ?
#ifdef RAYLIB_OP_OVERLOADS_RAYGUI
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#endif

#include <iostream>   //For stream insertion (operator<<) overloading, e.g, cout
#include <stdexcept>  //For divide-by-zero error trapping
#include <string>     //For PixelFormatNumberToName()

// **************************************************************
//
//      C++ Operator Overloads for RayLib
//
// **************************************************************
//
// RayLib is Copyright (c) 2013-2021 Ramon Santamaria (@raysan5)
// https://github.com/raysan5/ Overloads by Eric Jenislawski
// https://github.com/ProfJski/
//
// These operator overloads are convenience functions of two kinds:
// (1) Arithmetic operator overloads for use on vectors, colors and matricies,
// etc.  These permit more natural and expressive syntax in C++. For example,
// one can write
// VectorA=Vector2Subtract(Vector2Scale(VectorB,2.0),Vector2Add(VectorC,VectorD))
// as VectorA=2.0*VectorB-(VectorA+VectorD). (2) Output stream operator
// overloads to facilitate printing values to cout or filestreams.
//
// --------------------------------------------------------------
//
//  SELECT YOUR OPTIONS USING THE DEFINE STATEMENTS BELOW
//
// (A) Vector format
//
// PRINT_VECTORS_WITH_PARENTHESES prints a vector like an ordered set.  Vector3
// {1,2,3} prints as (1,2,3).  Colors print as (255,255,255,255) in RGBA order.
// PRINT_VECTORS_BY_COMPONENT prints a Vector3 {1,2,3} like so: x=1, y=2, z=3.
// Colors like so: R=255, G=255, B=255, A=255
//
// (B) Equality Operator
//
// Operator== determines whether Vector A equals Vector B.  The evaluation is
// straightforward with integer types, but not with floats. Three options are
// provided for the floating point types Vector2 and Vector3
//
// EQUALITY_OPERATOR_SIMPLE: Evaluates VectorA==VectorB as true IFF a.x==b.x and
// a.y==b.y, etc.  The overload merely invokes how operator== is defined for
// floats in one's C++ implementation. EQUALITY_OPERATOR_KNUTH: Uses C++ machine
// epsilon from std::numeric_limits to establish inequality if two quantities
// are close enough to be considered equal given the machine's precision and the
// magnitude of the floats.  From https://stackoverflow.com/a/253874, which in
// turn cites Knuth NONE: Comment out both options and the equality operator
// will not be overloaded at all.  Attempts to evaluate VectorA==VectorB will
// not compile.
//
// To see the difference between _SIMPLE and _KNUTH, try this test: Vector3
// v1={1.0,1.5,2.0}; v2=v1; v2*=sqrt(2.0); v2/=sqrt(2.0); Does v1==v2 ?  With
// _SIMPLE no; with _KNUTH, yes.

#define PRINT_VECTORS_WITH_PARENTHESES
//#define PRINT_VECTORS_BY_COMPONENT

//#define EQUALITY_OPERATOR_SIMPLE
#define EQUALITY_OPERATOR_KNUTH

// **************************************
//

// Minimal Color operators only, Vector operators provided by raylib 5.5 raymath.h
inline Color operator*(const Color& a, const Color& b) {
    int red = (int) a.r * (int) b.r / 255;
    int green = (int) a.g * (int) b.g / 255;
    int blue = (int) a.b * (int) b.b / 255;
    int alpha = (int) a.a * (int) b.a / 255;
    Color c; c.r = (unsigned char) red; c.g = (unsigned char) green; c.b = (unsigned char) blue; c.a = (unsigned char) alpha; return c;
}
inline Color& operator*=(Color& a, const Color& b) { a = a * b; return a; }
inline Color operator*(const Color& a, const float b) {
    Color c; c.r = (unsigned char)((int)a.r * b); c.g = (unsigned char)((int)a.g * b); c.b = (unsigned char)((int)a.b * b); c.a = a.a; return c;
}
inline Color& operator*=(Color& a, const float b) { a = a * b; return a; }
inline Color operator+(const Color& a, const Color& b) {
    Color c; c.r = (unsigned char)( (int)a.r + (int)b.r > 255 ? 255 : (int)a.r + (int)b.r ); c.g = (unsigned char)( (int)a.g + (int)b.g > 255 ? 255 : (int)a.g + (int)b.g ); c.b = (unsigned char)( (int)a.b + (int)b.b > 255 ? 255 : (int)a.b + (int)b.b ); c.a = (unsigned char)( (int)a.a + (int)b.a > 255 ? 255 : (int)a.a + (int)b.a ); return c;
}
inline Color& operator+=(Color& a, const Color& b) { a = a + b; return a; }
inline Color operator-(const Color& a, const Color& b) {
    Color c; int r = (int)a.r - (int)b.r; int g = (int)a.g - (int)b.g; int b2 = (int)a.b - (int)b.b; int alpha = (int)a.a - (int)b.a; c.r = (unsigned char)(r<0?0:r); c.g = (unsigned char)(g<0?0:g); c.b = (unsigned char)(b2<0?0:b2); c.a = (unsigned char)(alpha<0?0:alpha); return c;
}
inline Color& operator-=(Color& a, const Color& b) { a = a - b; return a; }
#endif
