/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2010 Orx-Project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file 13_Text.c
 * @date 12/03/2017
 * @author sam@sbseltzer.net
 *
 * Object creation tutorial
 */


#include "orx.h"


/* This is a basic C tutorial creating a viewport and an object.
 *
 * As orx is data driven, here we just write 2 lines of code to create a viewport
 * and an object. All their properties are defined in the config file (13_Text.ini).
 * As a matter of fact, the viewport is associated with a camera implicitly created from the
 * info given in the config file. You can also set their sizes, positions, the object colors,
 * scales, rotations, animations, physical properties, and so on. You can even request
 * random values for these without having to add a single line of code.
 * In a later tutorial we'll see how to generate your whole scene (all background
 * and landscape objects for example) with a simple for loop written in 3 lines of code.
 *
 * For now, you can try to uncomment some of the lines of 13_Text.ini, play with them,
 * then relaunch this tutorial. For an exhaustive list of options, please look at CreationTemplate.ini.
 */

orxTEXT *pstTestText = orxNULL;

static void TestMarkerTraversal(orxTEXT *_pstText, orxBITMAP *pstBitmap, orxCHARACTER_MAP *pstMap)
{
  const orxSTRING zString = orxText_GetString(_pstText);
  orxLINKLIST stStack = {0};
  orxHANDLE hMarker = orxNULL;
  orxHANDLE hFallback = orxNULL;
  orxHANDLE hIterator = orxText_GetMarkerIterator(_pstText);
  orxLOG("Testing markers for %s", zString);
  for (orxU32 u32Index = 0; u32Index < orxString_GetLength(zString); u32Index++)
  {
    for (;
         (hIterator != orxNULL) && (hIterator != orxHANDLE_UNDEFINED) && (orxText_GetMarkerIndex(hIterator) == u32Index) ;
         hIterator = orxText_NextMarker(hIterator))
    {
      orxTEXT_MARKER_TYPE eType = orxText_GetMarkerType(hIterator);
      hFallback = orxNULL;
      switch(eType)
      {
      case orxTEXT_MARKER_TYPE_NONE:
      {
        orxLOG("No marker @%u?", u32Index);
        break;
      }
      case orxTEXT_MARKER_TYPE_COLOR:
      {
        orxRGBA stColor = {0};
        if (orxText_GetMarkerColor(hIterator, &stColor) == orxSTATUS_SUCCESS)
        {
          orxLOG("Hit %s Marker @%u (%u, %u, %u, %u)", "color", u32Index, stColor.u8R, stColor.u8G, stColor.u8B, stColor.u8A);
          orxLinkList_AddEnd(&stStack, hIterator);
        }
        break;
      }
      case orxTEXT_MARKER_TYPE_FONT:
      {
        const orxFONT *pstFont;
        if (orxText_GetMarkerFont(hIterator, &pstFont) == orxSTATUS_SUCCESS)
        {
          orxLOG("Hit %s Marker @%u %s", "font", u32Index, orxFont_GetName(pstFont));
          orxLinkList_AddEnd(&stStack, hIterator);
        }
        break;
      }
      case orxTEXT_MARKER_TYPE_SCALE:
      {
        orxVECTOR vScale = {0};
        if (orxText_GetMarkerScale(hIterator, &vScale) == orxSTATUS_SUCCESS)
        {
          orxLOG("Hit %s Marker @%u (%d, %d, %d)", "scale", u32Index, vScale.fX, vScale.fY, vScale.fZ);
          orxLinkList_AddEnd(&stStack, hIterator);
        }
        break;
      }
      case orxTEXT_MARKER_TYPE_POP:
        hFallback = orxText_GetMarkerFallback(orxLinkList_GetLast(&stStack));
        if (orxLinkList_GetCounter(&stStack) > 0)
        {
          orxLinkList_Remove(orxLinkList_GetLast(&stStack));
        }
        break;
      case orxTEXT_MARKER_TYPE_CLEAR:
        hFallback = orxNULL;
        orxLinkList_Clean(&stStack);
        break;
      default:
        orxLOG("Unknown marker @%u?", u32Index);
      }
      hMarker = orxLinkList_GetLast(&stStack);
      if ((hMarker != orxNULL) && (hMarker != orxHANDLE_UNDEFINED))
      {
        /* orxLOG("Marker is in use"); */
        orxU32 u32LastMarkerIndex = orxText_GetMarkerIndex(hMarker);
        eType = orxText_GetMarkerType(hMarker);
        switch(eType)
        {
        case orxTEXT_MARKER_TYPE_COLOR:
        {
          orxRGBA stColor = {0};
          if (orxText_GetMarkerColor(hMarker, &stColor) == orxSTATUS_SUCCESS)
          {
            orxLOG("Using %s Marker @%u (%u, %u, %u, %u)", "color", u32LastMarkerIndex, stColor.u8R, stColor.u8G, stColor.u8B, stColor.u8A);
          }
          break;
        }
        case orxTEXT_MARKER_TYPE_FONT:
        {
          const orxFONT *pstFont;
          if (orxText_GetMarkerFont(hMarker, &pstFont) == orxSTATUS_SUCCESS)
          {
            orxLOG("Using %s Marker @%u %s", "font", u32LastMarkerIndex, orxFont_GetName(pstFont));
          }
          break;
        }
        case orxTEXT_MARKER_TYPE_SCALE:
        {
          orxVECTOR vScale = {0};
          if (orxText_GetMarkerScale(hMarker, &vScale) == orxSTATUS_SUCCESS)
          {
            orxLOG("Using %s Marker @%u (%d, %d, %d)", "scale", u32LastMarkerIndex, vScale.fX, vScale.fY, vScale.fZ);
          }
          break;
        }
        default:
          orxLOG("Invalid marker type @%u", u32LastMarkerIndex);
        }
        if ((hFallback != orxNULL) && (hFallback != orxHANDLE_UNDEFINED))
        {
          /* orxLOG("Marker fallback exists - applying changes..."); */
          orxU32 u32LastMarkerIndex = orxText_GetMarkerIndex(hFallback);
          eType = orxText_GetMarkerType(hFallback);
          switch(eType)
          {
          case orxTEXT_MARKER_TYPE_COLOR:
          {
            orxRGBA stColor = {0};
            if (orxText_GetMarkerColor(hFallback, &stColor) == orxSTATUS_SUCCESS)
            {
              orxLOG("Apply fallback %s Marker @%u (%u, %u, %u, %u)", "color", u32LastMarkerIndex, stColor.u8R, stColor.u8G, stColor.u8B, stColor.u8A);
            }
            break;
          }
          case orxTEXT_MARKER_TYPE_FONT:
          {
            const orxFONT *pstFont = orxNULL;
            if (orxText_GetMarkerFont(hFallback, &pstFont) == orxSTATUS_SUCCESS)
            {
              orxLOG("Apply fallback %s Marker @%u %s", "font", u32LastMarkerIndex, orxFont_GetName(pstFont));
            }
            break;
          }
          case orxTEXT_MARKER_TYPE_SCALE:
          {
            orxVECTOR vScale = {0};
            if (orxText_GetMarkerScale(hFallback, &vScale) == orxSTATUS_SUCCESS)
            {
              orxLOG("Apply fallback %s Marker @%u (%d, %d, %d)", "scale", u32LastMarkerIndex, vScale.fX, vScale.fY, vScale.fZ);
            }
            break;
          }
          default:
            orxLOG("Invalid marker type @%u", u32LastMarkerIndex);
          }
        }
      }
    }
  }
  orxLinkList_Clean(&stStack);
}

orxSTATUS orxFASTCALL ConfigEventHandler(const orxEVENT *_pstEvent) {
  orxSTATUS eResult = orxSTATUS_SUCCESS;
  if (_pstEvent->eID == orxRESOURCE_EVENT_UPDATE) {
    orxConfig_PushSection(orxText_GetName(pstTestText));
    orxText_SetString(pstTestText, orxConfig_GetString("String"));
    orxConfig_PopSection();
    TestMarkerTraversal(pstTestText, orxNULL, orxNULL);
  }
  return eResult;
}

/** Inits the tutorial
 */
orxSTATUS orxFASTCALL Init()
{
  /* Displays a small hint in console */
  orxLOG("\n* This tutorial creates a viewport/camera couple and multiple objects that display text"
         "\n* You can play with the config parameters in ../13_Text.ini"
         "\n* After changing them, relaunch the tutorial to see their effects");

  orxEvent_AddHandler(orxEVENT_TYPE_RESOURCE, ConfigEventHandler);
  /* Creates viewport */
  orxViewport_CreateFromConfig("Viewport");

  /* Creates object */
  orxObject_CreateFromConfig("Scene");

  orxOBJECT *pstMyTextObject = orxObject_CreateFromConfig("TextObject");
  orxGRAPHIC *pstGraphic = orxGRAPHIC(orxOBJECT_GET_STRUCTURE( pstMyTextObject, GRAPHIC) ) ;
	orxSTRUCTURE *pstStructure = orxGraphic_GetData( pstGraphic );
  pstTestText = orxTEXT(pstStructure);
  TestMarkerTraversal(pstTestText, orxNULL, orxNULL);
  /* Done! */
  return orxSTATUS_SUCCESS;
}

/** Run function
 */
orxSTATUS orxFASTCALL Run()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Should quit? */
  if(orxInput_IsActive("Quit"))
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
    orxLOG("TEST");
  }

  /* Done! */
  return eResult;
}

/** Exit function
 */
void orxFASTCALL Exit()
{
  /* We're a bit lazy here so we let orx clean all our mess! :) */
}

/** Main function
 */
int main(int argc, char **argv)
{
  /* Executes a new instance of tutorial */
  orx_Execute(argc, argv, Init, Run, Exit);

  return EXIT_SUCCESS;
}


#ifdef __orxMSVC__

// Here's an example for a console-less program under windows with visual studio
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // Inits and executes orx
  orx_WinExecute(Init, Run, Exit);

  // Done!
  return EXIT_SUCCESS;
}

#endif // __orxMSVC__
