// This file is part of the Clay3DS project.
//
// (c) 2025 Tommaso Dimatore
//
// For the full copyright and license information, please view the LICENSE
// file that was distributed with this source code.

#ifndef __CLAY3DS_H
#define __CLAY3DS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include "clay.h"

#define Clay3DSi__CLAY_COLOR_TO_C2D(cc) C2D_Color32((u8)cc.r, (u8)cc.g, (u8)cc.b, (u8)cc.a)
#define Clay3DSi__DEG_TO_RAD(value) ((value) * (M_PI / 180.f))
#define Clay3DSi__GET_TEXT_SCALE(fs) ((float)(fs) / 30.f)

static void Clay3DSi__FillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, u32 color)
{
  C2D_DrawTriangle(x1, y1, color, x2, y2, color, x3, y3, color, 0.f);
  C2D_DrawTriangle(x1, y1, color, x3, y3, color, x4, y4, color, 0.f);
}

static void Clay3DSi__DrawArc(float cx, float cy, float radius, float angs, float ange, float thickness, u32 color)
{
  u32 segments = 4;
  float ir = radius - thickness / 2.f;
  float or = radius + thickness / 2.f;

  float step = Clay3DSi__DEG_TO_RAD((ange - angs) / segments);
  float cosStep = cos(step);
  float sinStep = sin(step);

  float angle = Clay3DSi__DEG_TO_RAD(angs);
  float cosAngle1 = cos(angle);
  float sinAngle1 = sin(angle);

  float xInner1 = cx + ir * cosAngle1;
  float yInner1 = cy + ir * sinAngle1;
  float xOuter1 = cx + or * cosAngle1;
  float yOuter1 = cy + or * sinAngle1;

  for (u32 i = 1; i <= segments; ++i)
  {
    float cosAngle2 = cosAngle1 * cosStep - sinAngle1 * sinStep;
    float sinAngle2 = sinAngle1 * cosStep + cosAngle1 * sinStep;
    float xInner2 = cx + ir * cosAngle2;
    float yInner2 = cy + ir * sinAngle2;
    float xOuter2 = cx + or * cosAngle2;
    float yOuter2 = cy + or * sinAngle2;

    Clay3DSi__FillQuad(xInner1, yInner1, xInner2, yInner2, xOuter2, yOuter2, xOuter1, yOuter1, color);

    cosAngle1 = cosAngle2;
    sinAngle1 = sinAngle2;
    xInner1 = xInner2;
    yInner1 = yInner2;
    xOuter1 = xOuter2;
    yOuter1 = yOuter2;
  }
}

static void Clay3DSi__FillArc(float cx, float cy, float radius, float angs, float ange, u32 color)
{
  u32 segments = 4;

  float step = Clay3DSi__DEG_TO_RAD((ange - angs) / segments);
  float cosStep = cos(step);
  float sinStep = sin(step);

  float angle = Clay3DSi__DEG_TO_RAD(angs);
  float cosAngle1 = cos(angle);
  float sinAngle1 = sin(angle);

  float x1 = cx + radius * cosAngle1;
  float y1 = cy + radius * sinAngle1;

  for (u32 i = 1; i <= segments; ++i)
  {
    float cosAngle2 = cosAngle1 * cosStep - sinAngle1 * sinStep;
    float sinAngle2 = sinAngle1 * cosStep + cosAngle1 * sinStep;
    float x2 = cx + radius * cosAngle2;
    float y2 = cy + radius * sinAngle2;

    C2D_DrawTriangle(cx, cy, color, x1, y1, color, x2, y2, color, 0.f);

    cosAngle1 = cosAngle2;
    sinAngle1 = sinAngle2;
    x1 = x2;
    y1 = y2;
  }
}

static Clay_Dimensions Clay3DS_MeasureText(Clay_String* string, Clay_TextElementConfig* config)
{
  float scale = Clay3DSi__GET_TEXT_SCALE(config->fontSize);

  // TODO: re-use the same buffer for performance.
  char* cloned = (char*)malloc(string->length + 1);
  memcpy(cloned, string->chars, string->length);
  cloned[string->length] = '\0';

  C2D_Text text;
  C2D_TextBuf buffer = C2D_TextBufNew(string->length + 1);
  C2D_TextParse(&text, buffer, cloned);
  C2D_TextOptimize(&text);

  Clay_Dimensions dimensions;
  C2D_TextGetDimensions(&text, scale, scale, &dimensions.width, &dimensions.height);

  C2D_TextBufDelete(buffer);
  free(cloned);
  return dimensions;
}

static void Clay3DS_Render(C3D_RenderTarget* renderTarget, Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands)
{
  for (u32 i = 0; i < renderCommands.length; i++)
  {
    Clay_RenderCommand* renderCommand = Clay_RenderCommandArray_Get(&renderCommands, i);
    Clay_BoundingBox box = renderCommand->boundingBox;

    switch (renderCommand->commandType)
    {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleElementConfig* config = renderCommand->config.rectangleElementConfig;
      u32 color = Clay3DSi__CLAY_COLOR_TO_C2D(config->color);
      float tlr = config->cornerRadius.topLeft;
      float trr = config->cornerRadius.topRight;
      float brr = config->cornerRadius.bottomRight;
      float blr = config->cornerRadius.bottomLeft;

      if (tlr <= 0.f && trr <= 0.f && brr <= 0.f && blr <= 0.f)
      {
        // If no rounding is used, fall back to the faster, simpler, rectangle drawing.
        C2D_DrawRectSolid(box.x, box.y, 0.f, box.width, box.height, color);
      }
      else
      {
        // Make sure that the rounding is not bigger than half of any side.
        float max = fminf(box.width, box.height) / 2.f;
        tlr = fminf(tlr, max);
        trr = fminf(trr, max);
        brr = fminf(brr, max);
        blr = fminf(blr, max);

        // clang-format off
        Clay3DSi__FillQuad(box.x + tlr, box.y,
                           box.x + box.width - trr, box.y,
                           box.x + box.width - trr, box.y + trr,
                           box.x + tlr, box.y + tlr,
                           color); // Top
        Clay3DSi__FillQuad(box.x + box.width - trr, box.y + trr,
                           box.x + box.width, box.y + trr,
                           box.x + box.width, box.y + box.height - brr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           color); // Right
        Clay3DSi__FillQuad(box.x + blr, box.y + box.height - blr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           box.x + box.width - brr, box.y + box.height,
                           box.x + blr, box.y + box.height,
                           color); // Bottom
        Clay3DSi__FillQuad(box.x, box.y + tlr,
                           box.x + tlr, box.y + tlr,
                           box.x + blr, box.y + box.height - blr,
                           box.x, box.y + box.height - blr,
                           color); // Left
        Clay3DSi__FillQuad(box.x + tlr, box.y + tlr,
                           box.x + box.width - trr, box.y + trr,
                           box.x + box.width - brr, box.y + box.height - brr,
                           box.x + blr, box.y + box.height - blr,
                           color); // Inner
        // clang-format on

        Clay3DSi__FillArc(box.x + tlr, box.y + tlr, tlr, 180.f, 270.f, color);                       // Top Left
        Clay3DSi__FillArc(box.x + box.width - trr, box.y + trr, trr, 270.f, 360.f, color);           // Top Right
        Clay3DSi__FillArc(box.x + box.width - brr, box.y + box.height - brr, brr, 0.f, 90.f, color); // Bottom Right
        Clay3DSi__FillArc(box.x + blr, box.y + box.height - blr, blr, 90.f, 180.f, color);           // Bottom Left
      }

      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderElementConfig* config = renderCommand->config.borderElementConfig;
      u32 tc = Clay3DSi__CLAY_COLOR_TO_C2D(config->top.color);
      u32 lc = Clay3DSi__CLAY_COLOR_TO_C2D(config->left.color);
      u32 rc = Clay3DSi__CLAY_COLOR_TO_C2D(config->right.color);
      u32 bc = Clay3DSi__CLAY_COLOR_TO_C2D(config->bottom.color);
      float lw = config->left.width;
      float tw = config->top.width;
      float rw = config->right.width;
      float bw = config->bottom.width;
      // Make sure that the rounding is not bigger than half of any side.
      float max = fminf(box.width, box.height) / 2.f;
      float tlr = fminf(config->cornerRadius.topLeft, max);
      float trr = fminf(config->cornerRadius.topRight, max);
      float brr = fminf(config->cornerRadius.bottomRight, max);
      float blr = fminf(config->cornerRadius.bottomLeft, max);

      if (config->top.width > 0.f)
      {
        C2D_DrawRectSolid(box.x + tlr, box.y, 0.f, box.width - tlr - trr, tw, tc);
      }
      if (config->left.width > 0.f)
      {
        C2D_DrawRectSolid(box.x, box.y + tlr, 0.f, lw, box.height - tlr - blr, lc);
      }
      if (config->right.width > 0.f)
      {
        C2D_DrawRectSolid(box.x + box.width - rw, box.y + trr, 0.f, rw, box.height - trr - brr, rc);
      }
      if (config->bottom.width > 0.f)
      {
        C2D_DrawRectSolid(box.x + brr, box.y + box.height - bw, 0.f, box.width - blr - brr, bw, bc);
      }
      if (tlr > 0.f)
      {
        Clay3DSi__DrawArc(box.x + tlr, box.y + tlr, tlr - tw / 2.f, 180.f, 270.f, tw, tc);
      }
      if (trr > 0.f)
      {
        Clay3DSi__DrawArc(box.x + box.width - trr, box.y + trr, trr - tw / 2.f, 270.f, 360.f, tw, tc);
      }
      if (blr > 0.f)
      {
        Clay3DSi__DrawArc(box.x + blr, box.y + box.height - blr, blr - bw / 2.f, 90.f, 180.f, bw, bc);
      }
      if (brr > 0.f)
      {
        Clay3DSi__DrawArc(box.x + box.width - brr, box.y + box.height - brr, brr - bw / 2.f, 0.f, 90.f, bw, bc);
      }

      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextElementConfig* config = renderCommand->config.textElementConfig;
      float scale = Clay3DSi__GET_TEXT_SCALE(config->fontSize);
      u32 color = Clay3DSi__CLAY_COLOR_TO_C2D(config->textColor);

      Clay_String string = renderCommand->text;
      // TODO: re-use the same buffer for performance.
      char* cloned = (char*)malloc(string.length + 1);
      memcpy(cloned, string.chars, string.length);
      cloned[string.length] = '\0';

      C2D_Text text;
      // TODO: re-use the same buffer for performance.
      C2D_TextBuf buffer = C2D_TextBufNew(string.length + 1);
      C2D_TextParse(&text, buffer, cloned);
      C2D_TextOptimize(&text);
      C2D_DrawText(&text, C2D_WithColor, box.x, box.y, 0.f, scale, scale, color);

      C2D_TextBufDelete(buffer);
      free(cloned);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      C2D_SceneBegin(renderTarget);
      C3D_SetScissor(GPU_SCISSOR_NORMAL, dimensions.height - box.height - box.y, dimensions.width - box.width - box.x, box.height + box.y,
                     dimensions.width - box.x);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
      C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
      break;
    }
    default: {
      fprintf(stderr, "error: unhandled render command: %d\n", renderCommand->commandType);
      exit(1);
    }
    }
  }
}

#endif // __CLAY3DS_H
