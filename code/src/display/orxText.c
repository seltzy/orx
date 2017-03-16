/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2017 Orx-Project
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
 * @file orxText.c
 * @date 02/12/2008
 * @author iarwain@orx-project.org
 *
 */


#include "display/orxText.h"

#include "memory/orxMemory.h"
#include "core/orxConfig.h"
#include "core/orxEvent.h"
#include "core/orxLocale.h"
#include "core/orxResource.h"
#include "math/orxVector.h"
#include "object/orxStructure.h"
#include "utils/orxHashTable.h"

#ifdef __orxMSVC__

#include <malloc.h>
#pragma warning(disable : 4200)

#endif /* __orxMSVC__ */

/** Module flags
 */
#define orxTEXT_KU32_STATIC_FLAG_NONE         0x00000000  /**< No flags */

#define orxTEXT_KU32_STATIC_FLAG_READY        0x00000001  /**< Ready flag */

#define orxTEXT_KU32_STATIC_MASK_ALL          0xFFFFFFFF  /**< All mask */

/** orxTEXT flags / masks
 */
#define orxTEXT_KU32_FLAG_NONE                0x00000000  /**< No flags */

#define orxTEXT_KU32_FLAG_INTERNAL            0x10000000  /**< Internal structure handlign flag */

#define orxTEXT_KU32_MASK_ALL                 0xFFFFFFFF  /**< All mask */


/** Misc defines
 */
#define orxTEXT_KZ_CONFIG_STRING              "String"
#define orxTEXT_KZ_CONFIG_FONT                "Font"

#define orxTEXT_KC_LOCALE_MARKER              '$'
#define orxTEXT_KC_MARKER_SYNTAX_OPEN         '<'
#define orxTEXT_KC_MARKER_SYNTAX_CLOSE        '>'
#define orxTEXT_KC_MARKER_SYNTAX_ASSIGN       '='
#define orxTEXT_KZ_MARKER_TYPE_FONT           "font"
#define orxTEXT_KZ_MARKER_TYPE_COLOR          "col"
#define orxTEXT_KZ_MARKER_TYPE_SCALE          "scale"
#define orxTEXT_KZ_MARKER_TYPE_POP            "!"
#define orxTEXT_KZ_MARKER_TYPE_CLEAR          "*"
/* TODO: Implement a clear-all-styles marker */

#define orxTEXT_KU32_BANK_SIZE                256         /**< Bank size */
#define orxTEXT_KU32_MARKER_BANK_SIZE         128         /**< Bank size */
#define orxTEXT_KU32_STYLE_BANK_SIZE          128         /**< Bank size */


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Marker format data
 *  Capable of being shared between multiple markers.
 *  TODO: Either reuse these between multiple markers, or integrate with orxTEXT_MARKER_CELL.
 */
typedef struct __orxTEXT_MARKER_DATA_t
{
  orxTEXT_MARKER_TYPE eType;
  union
  {
    const orxFONT       *pstFont;
    orxRGBA              stRGBA;
    orxVECTOR            vScale;
  };
} orxTEXT_MARKER_DATA;

/** Marker position data
 *  Where the marker resides in an orxTEXT string.
 */
typedef struct __orxTEXT_MARKER_CELL_t
{
  orxU32                                 u32Index;
  const orxTEXT_MARKER_DATA             *pstData;
  const struct __orxTEXT_MARKER_CELL_t  *pstNext;
} orxTEXT_MARKER_CELL;

/** Marker stack entry
 *  Used specifically for dry run of marker traversal
 */
typedef struct __orxTEXT_MARKER_STACK_ENTRY_t
{
  orxLINKLIST_NODE            stNode;
  const orxTEXT_MARKER_DATA  *pstData;
  const orxTEXT_MARKER_DATA  *pstFallbackData;
} orxTEXT_MARKER_STACK_ENTRY;

/** Text structure
 */
struct __orxTEXT_t
{
  orxSTRUCTURE      stStructure;                /**< Public structure, first structure member : 32 */
  orxFONT          *pstFont;                    /**< Font : 20 */
  orxBANK          *pstMarkerDatas;
  orxBANK          *pstMarkerCells;
  const orxSTRING   zString;                    /**< String : 24 */
  orxFLOAT          fWidth;                     /**< Width : 28 */
  orxFLOAT          fHeight;                    /**< Height : 32 */
  const orxSTRING   zReference;                 /**< Config reference : 36 */
};

/** Static structure
 */
typedef struct __orxTEXT_STATIC_t
{
  orxU32        u32Flags;                       /**< Control flags : 4 */

} orxTEXT_STATIC;


/***************************************************************************
 * Module global variable                                                  *
 ***************************************************************************/

static orxTEXT_STATIC sstText;


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

/** Gets corresponding locale key
 * @param[in]   _pstText    Concerned text
 * @param[in]   _zProperty  Property to get
 * @return      orxSTRING / orxNULL
 */
static orxINLINE const orxSTRING orxText_GetLocaleKey(const orxTEXT *_pstText, const orxSTRING _zProperty)
{

  const orxSTRING zResult = orxNULL;

  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has reference? */
  if(_pstText->zReference != orxNULL)
  {
    const orxSTRING zString;

    /* Pushes its section */
    orxConfig_PushSection(_pstText->zReference);

    /* Gets its string */
    zString = orxConfig_GetString(_zProperty);

    /* Valid? */
    if(zString != orxNULL)
    {
      /* Begins with locale marker? */
      if((*zString == orxTEXT_KC_LOCALE_MARKER) && (*(zString + 1) != orxTEXT_KC_LOCALE_MARKER))
      {
        /* Updates result */
        zResult = zString + 1;
      }
    }

    /* Pops config section */
    orxConfig_PopSection();
  }

  /* Done! */
  return zResult;
}

static void orxFASTCALL orxText_CheckMarkerType(const orxSTRING _zCheckTypeName, orxTEXT_MARKER_TYPE eResultType, const orxSTRING _zAtString, orxTEXT_MARKER_TYPE *_peType, const orxSTRING *_pzNextToken)
{
  /* No bad input allowed! */
  orxASSERT(_peType != orxNULL);
  orxASSERT((_zCheckTypeName != orxNULL) && (_zCheckTypeName != orxSTRING_EMPTY) && (*_zCheckTypeName != orxCHAR_NULL));
  orxASSERT((_zAtString != orxNULL) && (_zAtString != orxSTRING_EMPTY) && (*_zAtString != orxCHAR_NULL)) ;
  orxASSERT(_pzNextToken != orxNULL);
  orxASSERT((*_pzNextToken != orxNULL) && (*_pzNextToken != orxSTRING_EMPTY) && (**_pzNextToken != orxCHAR_NULL));
  /* See _zCheckTypeName matches the start of _zAtString */
  orxU32 u32TypeLength = orxString_GetLength(_zCheckTypeName);
  if (orxString_NCompare(_zAtString, _zCheckTypeName, u32TypeLength) == 0) {
    /* Update the next token to be the end of the type name in _zAtString */
    *_pzNextToken = orxString_SkipWhiteSpaces(_zAtString + u32TypeLength);
    if (**_pzNextToken == orxTEXT_KC_MARKER_SYNTAX_ASSIGN) {
      *_peType = eResultType;
    }
  }
}

static orxTEXT_MARKER_DATA *orxFASTCALL orxText_CreateMarkerData(const orxTEXT *_pstText, orxTEXT_MARKER_TYPE _eType)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_eType != orxTEXT_MARKER_TYPE_NONE);
  /* Allocate and initialize marker data */
  orxTEXT_MARKER_DATA *pstResult = (orxTEXT_MARKER_DATA *) orxBank_Allocate(_pstText->pstMarkerDatas);
  orxASSERT(pstResult != orxNULL);
  pstResult->eType = _eType;
  return pstResult;
}

static orxTEXT_MARKER_STACK_ENTRY *orxFASTCALL orxText_AddMarkerStackEntry(orxLINKLIST *_pstStack, orxBANK *_pstStackBank, const orxTEXT_MARKER_DATA *_pstData, const orxTEXT_MARKER_DATA *_pstFallbackData)
{
  orxASSERT(_pstStackBank != orxNULL);
  orxASSERT(_pstData != orxNULL);
  /* Allocate and initialize marker stack entry */
  orxTEXT_MARKER_STACK_ENTRY *pstResult = (orxTEXT_MARKER_STACK_ENTRY *) orxBank_Allocate(_pstStackBank);
  orxASSERT(pstResult != orxNULL);
  pstResult->pstData = _pstData;
  pstResult->pstFallbackData = _pstFallbackData;
  orxLinkList_AddEnd(_pstStack, (orxLINKLIST_NODE *) pstResult);
  return pstResult;
}

static orxTEXT_MARKER_CELL *orxFASTCALL orxText_AddMarkerCell(const orxTEXT *_pstText, orxU32 _u32Index, const orxTEXT_MARKER_DATA *_pstData)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_u32Index != orxU32_UNDEFINED);

  /* Allocate and initialize marker call */
  orxTEXT_MARKER_CELL *pstResult = (orxTEXT_MARKER_CELL *) orxBank_Allocate(_pstText->pstMarkerCells);
  orxASSERT(pstResult != orxNULL);
  pstResult->u32Index = _u32Index;
  pstResult->pstData = _pstData;
  pstResult->pstNext = orxNULL;

  /* Update the previous marker, setting its 'next' to this marker */
  orxTEXT_MARKER_CELL *pstPrevMarker = orxBank_GetAtIndex(_pstText->pstMarkerCells, orxBank_GetIndex(_pstText->pstMarkerCells, pstResult) - 1);
  if (pstPrevMarker != orxNULL)
  {
    pstPrevMarker->pstNext = pstResult;
  }

  return pstResult;
}
static void orxFASTCALL orxText_PushMarker(const orxTEXT *_pstText, orxU32 _u32Index,
                                           const orxTEXT_MARKER_DATA **_ppstFallbackData, orxTEXT_MARKER_DATA *_pstData,
                                           orxLINKLIST *_pstStack, orxBANK *_pstStackBank)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_u32Index != orxU32_UNDEFINED);
  orxASSERT(_ppstFallbackData != orxNULL);
  orxASSERT(_pstStack != orxNULL);
  orxASSERT(_pstStackBank != orxNULL);

  /* Push data to stack with fallback data */
  orxTEXT_MARKER_STACK_ENTRY *pstStackEntry = orxText_AddMarkerStackEntry(_pstStack, _pstStackBank, _pstData, *_ppstFallbackData);
  /* Add a marker */
  orxTEXT_MARKER_CELL *pstMarker = orxText_AddMarkerCell(_pstText, _u32Index, _pstData);
  /* Update the processor's fallback data */
  *_ppstFallbackData = _pstData;
}
static void orxFASTCALL orxText_PopMarker(const orxTEXT *_pstText, orxU32 _u32Index,
                                          const orxTEXT_MARKER_DATA **_ppstFallbackData, orxLINKLIST *_pstStack)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_u32Index != orxU32_UNDEFINED);
  orxASSERT(_ppstFallbackData != orxNULL);
  orxASSERT(_pstStack != orxNULL);

  /* Pop the stack */
  orxTEXT_MARKER_STACK_ENTRY *pstPoppedEntry = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(_pstStack);
  orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedEntry);
  const orxTEXT_MARKER_DATA *pstData = pstPoppedEntry->pstFallbackData;
  /* Add a new marker with the fallback data of the popped marker */
  orxText_AddMarkerCell(_pstText, _u32Index, pstData);
  /* Update the processor's fallback data to be the newly added marker's data (i.e. popped marker's fallback data) */
  *_ppstFallbackData = pstData;
}

static const orxSTRING orxFASTCALL orxText_ProcessMarkedString(orxTEXT *_pstText, const orxSTRING _zString)
{
  const orxSTRING zMarkedString = orxNULL;
  const orxSTRING zResult       = orxNULL;
  orxSTRING zCleanedString      = orxNULL;
  orxU32 u32CleanedSize = 0, u32CleanedLength = 0;

  /* These are used for keeping track of marker type fallbacks */
  const orxTEXT_MARKER_DATA *pstPrevFont  = orxNULL;
  const orxTEXT_MARKER_DATA *pstPrevColor = orxNULL;
  const orxTEXT_MARKER_DATA *pstPrevScale = orxNULL;

  /* Used for a dry run of marker traversal */
  orxBANK      *pstDryRunBank = orxNULL;
  orxLINKLIST   stDryRunStack = {0};

  /* Clear banks */
  orxBank_Clear(_pstText->pstMarkerCells);
  orxBank_Clear(_pstText->pstMarkerDatas);

  /* If string is invalid, return it. */
  if (_zString == orxNULL || _zString == orxSTRING_EMPTY)
  {
    return _zString;
  }

  /* Initialize string traversal/storage variables */
  zMarkedString    = _zString;
  u32CleanedSize   = orxString_GetLength(zMarkedString) + 1;
  u32CleanedLength = 0;
  zCleanedString   = (orxSTRING) orxMemory_Allocate(u32CleanedSize * sizeof(orxCHAR), orxMEMORY_TYPE_MAIN);
  orxASSERT(zCleanedString != orxNULL);
  orxMemory_Zero(zCleanedString, u32CleanedSize * sizeof(orxCHAR));
  pstDryRunBank    = orxBank_Create(orxTEXT_KU32_STYLE_BANK_SIZE, sizeof(orxTEXT_MARKER_STACK_ENTRY),
                                    orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);

  /* TODO: Implement escapes for markers? */
  while ((zMarkedString != orxNULL) && (zMarkedString != orxSTRING_EMPTY) && (*zMarkedString != orxCHAR_NULL))
  {
    orxU32 u32StoreLength = 0;

    /* Find next marker open */
    const orxSTRING zMarkerStart = orxString_SearchChar(zMarkedString, orxTEXT_KC_MARKER_SYNTAX_OPEN);
    /* If not found, store remainder of string as clean text and break */
    if (zMarkerStart == orxNULL) {
      u32StoreLength = u32CleanedSize - u32CleanedLength;
      orxString_NCopy(zCleanedString + u32CleanedLength, zMarkedString, u32StoreLength);
      u32CleanedLength += u32StoreLength;
      break;
    }

    /* Looks like we might have a marker! */

    /* Store preceeding unstored text as clean text */
    u32StoreLength = (orxU32) (zMarkerStart - zMarkedString);
    orxString_NCopy(zCleanedString + u32CleanedLength, zMarkedString, u32StoreLength);
    u32CleanedLength += u32StoreLength;
    /* Move marked string forward */
    zMarkedString = zMarkerStart;

    /* Find next marker close */
    const orxSTRING zMarkerEnd = orxString_SearchChar(zMarkedString, orxTEXT_KC_MARKER_SYNTAX_CLOSE);
    /* If not found, store remainder of string as clean text and break */
    if (zMarkerEnd == orxNULL) {
      u32StoreLength = u32CleanedSize - u32CleanedLength;
      orxString_NCopy(zCleanedString + u32CleanedLength, zMarkedString, u32StoreLength);
      u32CleanedLength += u32StoreLength;
      break;
    }

    /* Looks a lot like a marker! Time to see if it's valid. */

    /* Marker open/close are valid, so we can safely analyze the marker string */
    const orxSTRING zMarkerTypeStart = orxString_SkipWhiteSpaces(zMarkedString + 1);
    orxTEXT_MARKER_TYPE eType = orxTEXT_MARKER_TYPE_NONE;

    /* TODO: Store style type lengths once before execution */
    orxSTRING zTestMarkerType = orxNULL;
    orxU32 u32TypeLength = 0;
    const orxSTRING zNextToken = orxSTRING_EMPTY;

    /* TODO: Reduce duplicate code here */

    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_FONT, orxTEXT_MARKER_TYPE_FONT, zMarkerTypeStart, &eType, &zNextToken);
      if (*zNextToken != orxTEXT_KC_MARKER_SYNTAX_ASSIGN) {
        eType = orxTEXT_MARKER_TYPE_NONE;
      }
    }
    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_COLOR, orxTEXT_MARKER_TYPE_COLOR, zMarkerTypeStart, &eType, &zNextToken);
      if (*zNextToken != orxTEXT_KC_MARKER_SYNTAX_ASSIGN) {
        eType = orxTEXT_MARKER_TYPE_NONE;
      }
    }
    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_SCALE, orxTEXT_MARKER_TYPE_SCALE, zMarkerTypeStart, &eType, &zNextToken);
      if (*zNextToken != orxTEXT_KC_MARKER_SYNTAX_ASSIGN) {
        eType = orxTEXT_MARKER_TYPE_NONE;
      }
    }
    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_CLEAR, orxTEXT_MARKER_TYPE_CLEAR, zMarkerTypeStart, &eType, &zNextToken);
      /* CLEAR doesn't have an assignment operator */
      if (zNextToken != zMarkerEnd) {
        eType = orxTEXT_MARKER_TYPE_NONE;
      }
    }
    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_POP, orxTEXT_MARKER_TYPE_POP, zMarkerTypeStart, &eType, &zNextToken);
      /* CLEAR doesn't have an assignment operator */
      if (zNextToken != zMarkerEnd) {
        eType = orxTEXT_MARKER_TYPE_NONE;
      }
    }

    /* If marker type is invalid, store marker as clean text, move marked string forward and continue */
    if (eType == orxTEXT_MARKER_TYPE_NONE) {
      /* TODO: I think there's a bug here where markers stored as clean text are missing their closing brace */
      u32StoreLength = (orxU32) (zMarkerEnd - zMarkedString);
      orxString_NCopy(zCleanedString + u32CleanedLength, zMarkedString, u32StoreLength);
      u32CleanedLength += u32StoreLength;
      zMarkedString = zMarkerEnd + 1;
      continue;
    }

    orxTEXT_MARKER_DATA *pstData = orxText_CreateMarkerData(_pstText, eType);

    /* Clear (i.e. pop everything) */
    if (eType == orxTEXT_MARKER_TYPE_CLEAR) {
      zMarkedString = zMarkerEnd + 1;
      /* Clear out the stack */
      while (orxLinkList_GetCounter(&stDryRunStack) > 0)
      {
        /* Inspect top of stack for what type needs to be rolled back */
        orxTEXT_MARKER_STACK_ENTRY *pstTop = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(&stDryRunStack);
        orxTEXT_MARKER_TYPE eTopType = pstTop->pstData->eType;
        /* Get a pointer to the appropriate fallback data */
        const orxTEXT_MARKER_DATA *pstFallbackData = orxNULL;
        switch(eTopType)
        {
        case orxTEXT_MARKER_TYPE_COLOR:
          pstFallbackData = pstPrevColor;
          break;
        case orxTEXT_MARKER_TYPE_FONT:
          pstFallbackData = pstPrevFont;
          break;
        case orxTEXT_MARKER_TYPE_SCALE:
          pstFallbackData = pstPrevScale;
          break;
        default:
          pstFallbackData = orxNULL;
        }
        /* Pop the stack, updating what pstFallbackData points to */
        orxText_PopMarker(_pstText, u32CleanedLength, &pstFallbackData, &stDryRunStack);
      }
      orxLinkList_Clean(&stDryRunStack);
      orxBank_Clear(pstDryRunBank);
      /* Let's see if this cleared out properly */
      orxASSERT(pstPrevColor == orxNULL);
      orxASSERT(pstPrevFont == orxNULL);
      orxASSERT(pstPrevScale == orxNULL);
      continue;
    }

    /* Pop marker stack */
    if (eType == orxTEXT_MARKER_TYPE_POP) {
      zMarkedString = zMarkerEnd + 1;
      /* We can't pop the stack if it's already empty */
      if (orxLinkList_GetCounter(&stDryRunStack) == 0)
      {
        continue;
      }
      /* Inspect top of stack for what type needs to be rolled back */
      orxTEXT_MARKER_STACK_ENTRY *pstTop = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(&stDryRunStack);
      orxTEXT_MARKER_TYPE eTopType = pstTop->pstData->eType;
      /* Get a pointer to the appropriate fallback data */
      const orxTEXT_MARKER_DATA *pstFallbackData = orxNULL;
      switch(eTopType)
      {
      case orxTEXT_MARKER_TYPE_COLOR:
        pstFallbackData = pstPrevColor;
        break;
      case orxTEXT_MARKER_TYPE_FONT:
        pstFallbackData = pstPrevFont;
        break;
      case orxTEXT_MARKER_TYPE_SCALE:
        pstFallbackData = pstPrevScale;
        break;
      default:
        pstFallbackData = orxNULL;
      }
      /* Pop the stack, updating what pstFallbackData points to */
      orxText_PopMarker(_pstText, u32CleanedLength, &pstFallbackData, &stDryRunStack);
      continue;
    }

    /* Not a pop or clear, so continue to populating pstData */

    /* Skip to the value */
    zNextToken = orxString_SkipWhiteSpaces(zNextToken + 1);

    /* Make a temporary string to hold the value alone */
    orxU32 u32ValueStringSize = (orxU32)(zMarkerEnd - zNextToken + 1) * (orxU32)sizeof(orxCHAR);

#ifdef __orxMSVC__

    orxCHAR *zValueString = (orxCHAR *)alloca(u32ValueStringSize * sizeof(orxCHAR));

#else /* __orxMSVC__ */

    orxCHAR zValueString[u32ValueStringSize];

#endif /* __orxMSVC__ */

    orxString_NCopy(zValueString, zNextToken, u32ValueStringSize);
    zValueString[u32ValueStringSize - 1] = orxCHAR_NULL;

    /* Check style values */

    if (eType == orxTEXT_MARKER_TYPE_FONT)
    {
      /* Attempt to store font style */
      const orxFONT *pstFont = orxFont_CreateFromConfig(zValueString);
      /* EDGE CASE: Handle invalid/missing font */
      if (pstFont == orxNULL)
      {
        pstFont = orxFont_GetDefaultFont();
      }
      orxASSERT(pstFont != orxNULL);
      /* If everything checks out, store the font and continue */
      pstData->pstFont = pstFont;
      orxText_PushMarker(_pstText, u32CleanedLength, &pstPrevFont, pstData, &stDryRunStack, pstDryRunBank);
    }
    else if (eType == orxTEXT_MARKER_TYPE_COLOR)
    {
      /* Attempt to store color style */
      orxVECTOR vColor = {0};
      /* EDGE CASE: Handle invalid/missing color */
      if (orxString_ToVector(zValueString, &vColor, orxNULL) == orxSTATUS_FAILURE)
      {
        /* TODO: We may want to use _pzRemaining for parsing an alpha value, if we choose to add it that way */
        orxVector_Set(&vColor, 1, 1, 1);
      }
      orxCOLOR stColor = {vColor, 1.0f};
      pstData->stRGBA = orxColor_ToRGBA(&stColor);
      orxText_PushMarker(_pstText, u32CleanedLength, &pstPrevColor, pstData, &stDryRunStack, pstDryRunBank);
    }
    else if (eType == orxTEXT_MARKER_TYPE_SCALE)
    {
      /* Attempt to store color style */
      orxVECTOR vScale = {0};
      /* EDGE CASE: Handle invalid/missing color */
      if (orxString_ToVector(zValueString, &vScale, orxNULL) == orxSTATUS_FAILURE)
      {
        vScale = orxVECTOR_1;
      }
      orxVector_Copy(&pstData->vScale, &vScale);
      orxText_PushMarker(_pstText, u32CleanedLength, &pstPrevScale, pstData, &stDryRunStack, pstDryRunBank);
    }
    else
    {
      /* Well this wasn't expected. Store marker as clean text, move marked string forward, and continue */
      u32StoreLength = (orxU32) (zMarkerEnd - zMarkedString);
      orxString_NCopy(zCleanedString + u32CleanedLength, zMarkedString, u32StoreLength);
      u32CleanedLength += u32StoreLength;
    }
    /* Move the marked string forward so we may continue */
    zMarkedString = zMarkerEnd + 1;
  }

  /* Free the dry run bank full of any remaining stack entries */
  orxBank_Delete(pstDryRunBank);

  /* Terminate cleaned string */
  zCleanedString[u32CleanedLength] = orxCHAR_NULL;

  /* Has new string? */
  if((zCleanedString != orxNULL) && (zCleanedString != orxSTRING_EMPTY))
  {
    /* Stores a duplicate */
    zResult = orxString_Store(zCleanedString);
  }
  /* Since the string is now stored internally, we can safely free it of its mortal coil */
  orxMemory_Free(zCleanedString);

  /* Done! */
  return zResult;
}

static orxSTATUS orxFASTCALL orxText_ProcessConfigData(orxTEXT *_pstText)
{
  const orxSTRING zString;
  const orxSTRING zName;
  orxSTATUS       eResult = orxSTATUS_FAILURE;

  /* Pushes its config section */
  orxConfig_PushSection(_pstText->zReference);

  /* Gets font name */
  zName = orxConfig_GetString(orxTEXT_KZ_CONFIG_FONT);

  /* Begins with locale marker? */
  if(*zName == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Gets its locale value */
    zName = (*(zName + 1) == orxTEXT_KC_LOCALE_MARKER) ? zName + 1 : orxLocale_GetString(zName + 1);
  }

  /* Valid? */
  if((zName != orxNULL) && (zName != orxSTRING_EMPTY))
  {
    orxFONT *pstFont;

    /* Creates font */
    pstFont = orxFont_CreateFromConfig(zName);

    /* Valid? */
    if(pstFont != orxNULL)
    {
      /* Stores it */
      if(orxText_SetFont(_pstText, pstFont) != orxSTATUS_FAILURE)
      {
        /* Sets its owner */
        orxStructure_SetOwner(pstFont, _pstText);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
      }
      else
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't set font (%s) for text (%s).", zName, _pstText->zReference);

        /* Sets default font */
        orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't create font (%s) for text (%s).", zName, _pstText->zReference);

      /* Sets default font */
      orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
    }
  }
  else
  {
    /* Sets default font */
    orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
  }

  /* Gets its string */
  zString = orxConfig_GetString(orxTEXT_KZ_CONFIG_STRING);

  /* Begins with locale marker? */
  if(*zString == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Stores its locale value */
    eResult = orxText_SetString(_pstText, (*(zString + 1) == orxTEXT_KC_LOCALE_MARKER) ? zString + 1 : orxLocale_GetString(zString + 1));
  }
  else
  {
    /* Stores raw text */
    eResult = orxText_SetString(_pstText, zString);
  }

  /* Pops config section */
  orxConfig_PopSection();

  /* Done! */
  return eResult;
}

/** Event handler
 * @param[in]   _pstEvent                     Sent event
 * @return      orxSTATUS_SUCCESS if handled / orxSTATUS_FAILURE otherwise
 */
static orxSTATUS orxFASTCALL orxText_EventHandler(const orxEVENT *_pstEvent)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Locale? */
  if(_pstEvent->eType == orxEVENT_TYPE_LOCALE)
  {
    /* Select language event? */
    if(_pstEvent->eID == orxLOCALE_EVENT_SELECT_LANGUAGE)
    {
      orxTEXT *pstText;

      /* For all texts */
      for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
          pstText != orxNULL;
          pstText = orxTEXT(orxStructure_GetNext(pstText)))
      {
        const orxSTRING zLocaleKey;

        /* Gets its corresponding locale string */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_STRING);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          const orxSTRING zText;

          /* Gets its localized value */
          zText = orxLocale_GetString(zLocaleKey);

          /* Valid? */
          if(*zText != orxCHAR_NULL)
          {
            /* Updates text */
            orxText_SetString(pstText, zText);
          }
        }

        /* Gets its corresponding locale font */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_FONT);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          orxFONT *pstFont;

          /* Creates font */
          pstFont = orxFont_CreateFromConfig(orxLocale_GetString(zLocaleKey));

          /* Valid? */
          if(pstFont != orxNULL)
          {
            /* Updates text */
            if(orxText_SetFont(pstText, pstFont) != orxSTATUS_FAILURE)
            {
              /* Sets its owner */
              orxStructure_SetOwner(pstFont, pstText);

              /* Updates flags */
              orxStructure_SetFlags(pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
            }
            else
            {
              /* Sets default font */
              orxText_SetFont(pstText, orxFONT(orxFont_GetDefaultFont()));
            }
          }
        }
      }
    }
  }
  /* Resource */
  else
  {
    /* Checks */
    orxASSERT(_pstEvent->eType == orxEVENT_TYPE_RESOURCE);

    /* Add or update? */
    if((_pstEvent->eID == orxRESOURCE_EVENT_ADD) || (_pstEvent->eID == orxRESOURCE_EVENT_UPDATE))
    {
      orxRESOURCE_EVENT_PAYLOAD *pstPayload;

      /* Gets payload */
      pstPayload = (orxRESOURCE_EVENT_PAYLOAD *)_pstEvent->pstPayload;

      /* Is config group? */
      if(pstPayload->u32GroupID == orxString_ToCRC(orxCONFIG_KZ_RESOURCE_GROUP))
      {
        orxTEXT *pstText;

        /* For all texts */
        for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
            pstText != orxNULL;
            pstText = orxTEXT(orxStructure_GetNext(pstText)))
        {
          /* Match origin? */
          if(orxConfig_GetOriginID(pstText->zReference) == pstPayload->u32NameID)
          {
            /* Re-processes its config data */
            orxText_ProcessConfigData(pstText);
          }
        }
      }
    }
  }

  /* Done! */
  return eResult;
}

/** Updates text size
 * @param[in]   _pstText      Concerned text
 */
static void orxFASTCALL orxText_UpdateSize(orxTEXT *_pstText)
{
  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has string and font? */
  if((_pstText->zString != orxNULL) && (_pstText->zString != orxSTRING_EMPTY) && (_pstText->pstFont != orxNULL))
  {
    orxFLOAT        fWidth, fMaxWidth, fHeight, fCharacterHeight;
    orxU32          u32CharacterCodePoint;
    const orxCHAR  *pc;

    /* Gets character height */
    fCharacterHeight = orxFont_GetCharacterHeight(_pstText->pstFont);

    /* For all characters */
    for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_pstText->zString, &pc), fHeight = fCharacterHeight, fWidth = fMaxWidth = orxFLOAT_0;
        u32CharacterCodePoint != orxCHAR_NULL;
        u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc))
    {
      /* Depending on character */
      switch(u32CharacterCodePoint)
      {
        case orxCHAR_CR:
        {
          /* Half EOL? */
          if(*pc == orxCHAR_LF)
          {
            /* Updates pointer */
            pc++;
          }

          /* Fall through */
        }

        case orxCHAR_LF:
        {
          /* Updates height */
          fHeight += fCharacterHeight;

          /* Updates max width */
          fMaxWidth = orxMAX(fMaxWidth, fWidth);

          /* Resets width */
          fWidth = orxFLOAT_0;

          break;
        }

        default:
        {
          /* Updates width */
          fWidth += orxFont_GetCharacterWidth(_pstText->pstFont, u32CharacterCodePoint);

          break;
        }
      }
    }

    /* Stores values */
    _pstText->fWidth  = orxMAX(fWidth, fMaxWidth);
    _pstText->fHeight = fHeight;
  }
  else
  {
    /* Clears values */
    _pstText->fWidth = _pstText->fHeight = orxFLOAT_0;
  }

  /* Done! */
  return;
}

/** Deletes all texts
 */
static orxINLINE void orxText_DeleteAll()
{
  orxTEXT *pstText;

  /* Gets first text */
  pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));

  /* Non empty? */
  while(pstText != orxNULL)
  {
    /* Deletes text */
    orxText_Delete(pstText);

    /* Gets first text */
    pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
  }

  return;
}


/***************************************************************************
 * Public functions                                                        *
 ***************************************************************************/

/** Setups the text module
 */
void orxFASTCALL orxText_Setup()
{
  /* Adds module dependencies */
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_MEMORY);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_CONFIG);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_EVENT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_FONT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_LOCALE);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_STRUCTURE);

  return;
}

/** Inits the text module
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Init()
{
  orxSTATUS eResult;

  /* Not already Initialized? */
  if(!(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY))
  {
    /* Cleans static controller */
    orxMemory_Zero(&sstText, sizeof(orxTEXT_STATIC));

    /* Adds event handler */
    eResult = orxEvent_AddHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Valid? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Registers structure type */
      eResult = orxSTRUCTURE_REGISTER(TEXT, orxSTRUCTURE_STORAGE_TYPE_LINKLIST, orxMEMORY_TYPE_MAIN, orxTEXT_KU32_BANK_SIZE, orxNULL);

      /* Success? */
      if(eResult != orxSTATUS_FAILURE)
      {
        /* Updates flags for screen text creation */
        sstText.u32Flags = orxTEXT_KU32_STATIC_FLAG_READY;

        /* Adds event handler for resources */
        orxEvent_AddHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
      }
      else
      {
        /* Removes event handler */
        orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);
      }
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to initialize text module when it was already initialized.");

    /* Already initialized */
    eResult = orxSTATUS_SUCCESS;
  }

  /* Not initialized? */
  if(eResult == orxSTATUS_FAILURE)
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Initializing text module failed.");

    /* Updates Flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }

  /* Done! */
  return eResult;
}

/** Exits from the text module
 */
void orxFASTCALL orxText_Exit()
{
  /* Initialized? */
  if(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY)
  {
    /* Deletes text list */
    orxText_DeleteAll();

    /* Removes event handlers */
    orxEvent_RemoveHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
    orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Unregisters structure type */
    orxStructure_Unregister(orxSTRUCTURE_ID_TEXT);

    /* Updates flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to exit text module when it wasn't initialized.");
  }

  return;
}

/** Creates an empty text
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_Create()
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);

  /* Creates text */
  pstResult = orxTEXT(orxStructure_Create(orxSTRUCTURE_ID_TEXT));

  /* Created? */
  if(pstResult != orxNULL)
  {
    /* Inits it */
    pstResult->zString    = orxNULL;
    pstResult->pstFont    = orxNULL;
    pstResult->pstMarkerDatas  = orxBank_Create(orxTEXT_KU32_STYLE_BANK_SIZE, sizeof(orxTEXT_MARKER_DATA),
                                           orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
    pstResult->pstMarkerCells = orxBank_Create(orxTEXT_KU32_MARKER_BANK_SIZE, sizeof(orxTEXT_MARKER_CELL),
                                           orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);

    /* Inits flags */
    orxStructure_SetFlags(pstResult, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_MASK_ALL);

    /* Increases counter */
    orxStructure_IncreaseCounter(pstResult);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to create structure for text.");
  }

  /* Done! */
  return pstResult;
}

/** Creates a text from config
 * @param[in]   _zConfigID    Config ID
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_CreateFromConfig(const orxSTRING _zConfigID)
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxASSERT((_zConfigID != orxNULL) && (_zConfigID != orxSTRING_EMPTY));

  /* Pushes section */
  if((orxConfig_HasSection(_zConfigID) != orxFALSE)
  && (orxConfig_PushSection(_zConfigID) != orxSTATUS_FAILURE))
  {
    /* Creates text */
    pstResult = orxText_Create();

    /* Valid? */
    if(pstResult != orxNULL)
    {
      /* Stores its reference key */
      pstResult->zReference = orxString_Store(orxConfig_GetCurrentSection());

      /* Processes its config data */
      if(orxText_ProcessConfigData(pstResult) == orxSTATUS_FAILURE)
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't process config data for text <%s>.", _zConfigID);

        /* Deletes it */
        orxText_Delete(pstResult);

        /* Updates result */
        pstResult = orxNULL;
      }
    }

    /* Pops previous section */
    orxConfig_PopSection();
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't find config section named (%s).", _zConfigID);

    /* Updates result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

/** Deletes a text
 * @param[in]   _pstText      Concerned text
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Delete(orxTEXT *_pstText)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Decreases counter */
  orxStructure_DecreaseCounter(_pstText);

  /* Not referenced? */
  if(orxStructure_GetRefCounter(_pstText) == 0)
  {
    /* Removes string */
    orxText_SetString(_pstText, orxNULL);

    /* Removes font */
    orxText_SetFont(_pstText, orxNULL);

    /* Deletes markers */
    orxBank_Delete(_pstText->pstMarkerCells);

    /* Deletes styles */
    orxBank_Delete(_pstText->pstMarkerDatas);

    /* Deletes structure */
    orxStructure_Delete(_pstText);
  }
  else
  {
    /* Referenced by others */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Gets text size
 * @param[in]   _pstText      Concerned text
 * @param[out]  _pfWidth      Text's width
 * @param[out]  _pfHeight     Text's height
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetSize(const orxTEXT *_pstText, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Updates result */
  *_pfWidth   = _pstText->fWidth;
  *_pfHeight  = _pstText->fHeight;

  /* Done! */
  return eResult;
}

/** Gets text name
 * @param[in]   _pstText      Concerned text
 * @return      Text name / orxNULL
 */
const orxSTRING orxFASTCALL orxText_GetName(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  zResult = (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY;

  /* Done! */
  return zResult;
}

/** Gets text string
 * @param[in]   _pstText      Concerned text
 * @return      Text string / orxSTRING_EMPTY
 */
const orxSTRING orxFASTCALL orxText_GetString(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has string? */
  if(_pstText->zString != orxNULL)
  {
    /* Updates result */
    zResult = _pstText->zString;
  }
  else
  {
    /* Updates result */
    zResult = orxSTRING_EMPTY;
  }

  /* Done! */
  return zResult;
}

/** Gets text font
 * @param[in]   _pstText      Concerned text
 * @return      Text font / orxNULL
 */
orxFONT *orxFASTCALL orxText_GetFont(const orxTEXT *_pstText)
{
  orxFONT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  pstResult = _pstText->pstFont;

  /* Done! */
  return pstResult;
}

/** Sets text string
 * @param[in]   _pstText      Concerned text
 * @param[in]   _zString      String to contain
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetString(orxTEXT *_pstText, const orxSTRING _zString)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has current string? */
  if((_pstText->zString != orxNULL) && (_pstText->zString != orxSTRING_EMPTY))
  {
    /* Cleans it */
    _pstText->zString = orxNULL;
  }

  /* Process markers out of the string */
  _zString = orxText_ProcessMarkedString(_pstText, _zString);

  /* Has new string? */
  if((_zString != orxNULL) && (_zString != orxSTRING_EMPTY))
  {
    /* Stores a duplicate */
    _pstText->zString = orxString_Store(_zString);
  }

  /* Updates text size */
  orxText_UpdateSize(_pstText);

  /* Done! */
  return eResult;
}

/** Sets text font
 * @param[in]   _pstText      Concerned text
 * @param[in]   _pstFont      Font / orxNULL to use default
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetFont(orxTEXT *_pstText, orxFONT *_pstFont)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Different? */
  if(_pstText->pstFont != _pstFont)
  {
    /* Has current font? */
    if(_pstText->pstFont != orxNULL)
    {
      /* Updates structure reference counter */
      orxStructure_DecreaseCounter(_pstText->pstFont);

      /* Internally handled? */
      if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL))
      {
        /* Removes its owner */
        orxStructure_SetOwner(_pstText->pstFont, orxNULL);

        /* Deletes it */
        orxFont_Delete(_pstText->pstFont);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_FLAG_INTERNAL);
      }

      /* Cleans it */
      _pstText->pstFont = orxNULL;
    }

    /* Has new font? */
    if(_pstFont != orxNULL)
    {
      /* Stores it */
      _pstText->pstFont = _pstFont;

      /* Updates its reference counter */
      orxStructure_IncreaseCounter(_pstFont);
    }

    /* Updates text's size */
    orxText_UpdateSize(_pstText);
  }

  /* Done! */
  return eResult;
}

orxU32 orxFASTCALL orxText_GetMarkerCounter(orxTEXT *_pstText)
{
  return orxBank_GetCounter(_pstText->pstMarkerCells);
}

orxHANDLE orxFASTCALL orxText_GetMarkerIterator(orxTEXT *_pstText)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxHANDLE hResult;
  if (_pstText != orxNULL)
  {
    pstCell = (orxTEXT_MARKER_CELL *) orxBank_GetAtIndex(_pstText->pstMarkerCells, 0);
    hResult = (orxHANDLE) pstCell;
  }
  else
  {
    hResult = orxHANDLE_UNDEFINED;
  }
  return hResult;
}

orxHANDLE orxFASTCALL orxText_NextMarker(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxHANDLE hResult;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    hResult = (orxHANDLE) pstCell->pstNext;
  }
  else
  {
    hResult = orxHANDLE_UNDEFINED;
  }
  return hResult;
}

orxTEXT_MARKER_TYPE orxFASTCALL orxText_GetMarkerType(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxTEXT_MARKER_TYPE eResult;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if (pstCell->pstData != orxNULL)
    {
      eResult = pstCell->pstData->eType;
    }
    else
    {
      eResult = orxTEXT_MARKER_TYPE_NONE;
    }
  }
  else
  {
    eResult = orxTEXT_MARKER_TYPE_NONE;
  }
  return eResult;
}

orxSTATUS orxFASTCALL orxText_GetMarkerFont(orxHANDLE _hIterator, orxFONT const **_ppstFont)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_FONT))
    {
      *_ppstFont = pstCell->pstData->pstFont;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_ppstFont = orxNULL;
  }
  return eResult;
}

orxSTATUS orxFASTCALL orxText_GetMarkerColor(orxHANDLE _hIterator, orxRGBA *_pstColor)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_COLOR))
    {
      *_pstColor = pstCell->pstData->stRGBA;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_pstColor = orx2RGBA(255, 255, 255, 255);
  }
  return eResult;
}

orxSTATUS orxFASTCALL orxText_GetMarkerScale(orxHANDLE _hIterator, orxVECTOR *_pstScale)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_SCALE))
    {
      *_pstScale = pstCell->pstData->vScale;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_pstScale = orxVECTOR_1;
  }
  return eResult;
}

orxU32 orxFASTCALL orxText_GetMarkerIndex(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxU32 u32Result;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    u32Result = pstCell->u32Index;
  }
  else
  {
    u32Result = orxU32_UNDEFINED;
  }
  return u32Result;
}

orxHANDLE orxFASTCALL orxText_WalkMarkers(orxHANDLE _hIterator, orxU32 _u32Index, orxLINKLIST *_pstStack, orxHANDLE *_phMarker, orxBOOL *_pstHasAnother)
{
  orxHANDLE hResult;
  /* Valid iterator? */
  if ((_hIterator != orxHANDLE_UNDEFINED) && (_hIterator != orxNULL) && (_u32Index != orxU32_UNDEFINED))
  {
    /* New marker? */
    if (orxText_GetMarkerIndex(_hIterator) == _u32Index)
    {
      orxTEXT_MARKER_TYPE eType = orxText_GetMarkerType(_hIterator);
      switch(eType)
      {
      /* Stack pushes */
      case orxTEXT_MARKER_TYPE_COLOR:
      case orxTEXT_MARKER_TYPE_FONT:
      case orxTEXT_MARKER_TYPE_SCALE:
      {
        orxLinkList_AddEnd(_pstStack, (orxLINKLIST_NODE *) _hIterator);
        break;
      }
      /* Stack pop */
      case orxTEXT_MARKER_TYPE_POP:
      {
        orxLinkList_Remove(orxLinkList_GetLast(_pstStack));
        break;
      }
      /* Stack clear */
      case orxTEXT_MARKER_TYPE_CLEAR:
      {
        orxLinkList_Clean(_pstStack);
        break;
      }
      /* Invalid types */
      case orxTEXT_MARKER_TYPE_NONE:
      default:
        break;
      }

      /* Move iterator forward */
      hResult = orxText_NextMarker(_hIterator);
    }
    else
    {
      /* Keep iterator same */
      hResult = _hIterator;
    }
  }
  else
  {
    /* Iterator provided was invalid */
    hResult = orxHANDLE_UNDEFINED;
    orxLinkList_Clean(_pstStack);
  }
  /* Update current marker */
  if (orxLinkList_GetCounter(_pstStack) != 0)
  {
    *_phMarker = (orxHANDLE) orxLinkList_GetLast(_pstStack);
  }
  else
  {
    *_phMarker = orxHANDLE_UNDEFINED;
  }
  *_pstHasAnother = ( (hResult != orxHANDLE_UNDEFINED) &&
                      (hResult != orxNULL) &&
                      (orxText_GetMarkerIndex(hResult) == _u32Index) );
  return hResult;
}
