/* Orx - Portable Game Engine
 *
 * Orx is the legal property of its developers, whose names
 * are listed in the COPYRIGHT file distributed
 * with this source distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file orxObject.c
 * @date 01/12/2003
 * @author iarwain@orx-project.org
 *
 * @todo
 */


#include "object/orxObject.h"

#include "debug/orxDebug.h"
#include "core/orxConfig.h"
#include "core/orxClock.h"
#include "core/orxEvent.h"
#include "memory/orxMemory.h"
#include "anim/orxAnimPointer.h"
#include "display/orxGraphic.h"
#include "display/orxText.h"
#include "physics/orxBody.h"
#include "object/orxFrame.h"
#include "object/orxSpawner.h"
#include "render/orxCamera.h"
#include "render/orxFXPointer.h"
#include "sound/orxSoundPointer.h"


/** Module flags
 */
#define orxOBJECT_KU32_STATIC_FLAG_NONE         0x00000000

#define orxOBJECT_KU32_STATIC_FLAG_READY        0x00000001
#define orxOBJECT_KU32_STATIC_FLAG_CLOCK        0x00000002
#define orxOBJECT_KU32_STATIC_FLAG_INTERNAL     0x00000004  /**< Internal flag */

#define orxOBJECT_KU32_STATIC_MASK_ALL          0xFFFFFFFF


/** Flags
 */
#define orxOBJECT_KU32_FLAG_NONE                0x00000000  /**< No flags */

#define orxOBJECT_KU32_FLAG_2D                  0x00000010  /**< 2D flag */
#define orxOBJECT_KU32_FLAG_HAS_COLOR           0x00000020  /**< Has color flag */
#define orxOBJECT_KU32_FLAG_ENABLED             0x10000000  /**< Enabled flag */
#define orxOBJECT_KU32_FLAG_RENDERED            0x20000000  /**< Rendered flag */
#define orxOBJECT_KU32_FLAG_SMOOTHING_ON        0x01000000  /**< Smoothing on flag  */
#define orxOBJECT_KU32_FLAG_SMOOTHING_OFF       0x02000000  /**< Smoothing off flag  */
#define orxOBJECT_KU32_FLAG_HAS_LIFETIME        0x04000000  /**< Has lifetime flag  */

#define orxOBJECT_KU32_FLAG_BLEND_MODE_NONE     0x00000000  /**< Blend mode no flags */

#define orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA    0x00100000  /**< Blend mode alpha flag */
#define orxOBJECT_KU32_FLAG_BLEND_MODE_MULTIPLY 0x00200000  /**< Blend mode multiply flag */
#define orxOBJECT_KU32_FLAG_BLEND_MODE_ADD      0x00400000  /**< Blend mode add flag */

#define orxOBJECT_KU32_MASK_BLEND_MODE_ALL      0x00F00000  /**< Blend mode mask */

#define orxOBJECT_KU32_MASK_ALL                 0xFFFFFFFF  /**< All mask */


#define orxOBJECT_KU32_STORAGE_FLAG_NONE        0x00000000

#define orxOBJECT_KU32_STORAGE_FLAG_INTERNAL    0x00000001

#define orxOBJECT_KU32_STORAGE_MASK_ALL         0xFFFFFFFF


/** Misc defines
 */
#define orxOBJECT_KU32_NEIGHBOR_LIST_SIZE       128

#define orxOBJECT_KZ_CONFIG_GRAPHIC_NAME        "Graphic"
#define orxOBJECT_KZ_CONFIG_ANIMPOINTER_NAME    "AnimationSet"
#define orxOBJECT_KZ_CONFIG_BODY                "Body"
#define orxOBJECT_KZ_CONFIG_SPAWNER             "Spawner"
#define orxOBJECT_KZ_CONFIG_PIVOT               "Pivot"
#define orxOBJECT_KZ_CONFIG_AUTO_SCROLL         "AutoScroll"
#define orxOBJECT_KZ_CONFIG_FLIP                "Flip"
#define orxOBJECT_KZ_CONFIG_COLOR               "Color"
#define orxOBJECT_KZ_CONFIG_ALPHA               "Alpha"
#define orxOBJECT_KZ_CONFIG_DEPTH_SCALE         "DepthScale"
#define orxOBJECT_KZ_CONFIG_POSITION            "Position"
#define orxOBJECT_KZ_CONFIG_SPEED               "Speed"
#define orxOBJECT_KZ_CONFIG_ROTATION            "Rotation"
#define orxOBJECT_KZ_CONFIG_ANGULAR_VELOCITY    "AngularVelocity"
#define orxOBJECT_KZ_CONFIG_SCALE               "Scale"
#define orxOBJECT_KZ_CONFIG_FX                  "FX"
#define orxOBJECT_KZ_CONFIG_SOUND               "Sound"
#define orxOBJECT_KZ_CONFIG_FREQUENCY           "AnimationFrequency"
#define orxOBJECT_KZ_CONFIG_SMOOTHING           "Smoothing"
#define orxOBJECT_KZ_CONFIG_BLEND_MODE          "BlendMode"
#define orxOBJECT_KZ_CONFIG_LIFETIME            "LifeTime"
#define orxOBJECT_KZ_CONFIG_PARENT_CAMERA       "ParentCamera"
#define orxOBJECT_KZ_CONFIG_USE_RELATIVE_SPEED  "UseRelativeSpeed"
#define orxOBJECT_KZ_CONFIG_USE_PARENT_SPACE    "UseParentSpace"

#define orxOBJECT_KZ_CENTERED_PIVOT             "centered"
#define orxOBJECT_KZ_X                          "x"
#define orxOBJECT_KZ_Y                          "y"
#define orxOBJECT_KZ_BOTH                       "both"
#define orxOBJECT_KZ_ALPHA                      "alpha"
#define orxOBJECT_KZ_MULTIPLY                   "multiply"
#define orxOBJECT_KZ_ADD                        "add"


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Object storage structure
 */
typedef struct __orxOBJECT_STORAGE_t
{
  orxSTRUCTURE *pstStructure;                   /**< Structure pointer : 4 */
  orxU32        u32Flags;                       /**< Flags : 8 */

} orxOBJECT_STORAGE;

/** Object structure
 */
struct __orxOBJECT_t
{
  orxSTRUCTURE      stStructure;                /**< Public structure, first structure member : 16 */
  orxOBJECT_STORAGE astStructure[orxSTRUCTURE_ID_LINKABLE_NUMBER]; /**< Stored structures : 72 */
  orxCOLOR          stColor;                    /**< Object color : 88 */
  orxVECTOR         vSpeed;                     /**< Object speed : 100 */
  orxVOID          *pUserData;                  /**< User data : 104 */
  orxSTRUCTURE     *pstOwner;                   /**< Owner structure : 108 */
  orxFLOAT          fAngularVelocity;           /**< Angular velocity : 112 */
  orxFLOAT          fLifeTime;                  /**< Life time : 116 */
  orxSTRING         zReference;                 /**< Config reference : 120 */

  /* Padding */
  orxPAD(120)
};

/** Static structure
 */
typedef struct __orxOBJECT_STATIC_t
{
  orxCLOCK *pstClock;                           /**< Clock */
  orxU32 u32Flags;                              /**< Control flags */

} orxOBJECT_STATIC;


/***************************************************************************
 * Static variables                                                        *
 ***************************************************************************/

/** Static data
 */
orxSTATIC orxOBJECT_STATIC sstObject;


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

/** Deletes all the objects
 */
orxSTATIC orxINLINE orxVOID orxObject_DeleteAll()
{
  orxOBJECT *pstObject;

  /* Gets first object */
  pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT));

  /* Non empty? */
  while(pstObject != orxNULL)
  {
    /* Deletes object */
    orxObject_Delete(pstObject);

    /* Gets first object */
    pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT));
  }

  return;
}

/** Updates all the objects
 * @param[in] _pstClockInfo       Clock information where this callback has been registered
 * @param[in] _pstContext         User defined context
 */
orxVOID orxFASTCALL orxObject_UpdateAll(orxCONST orxCLOCK_INFO *_pstClockInfo, orxVOID *_pstContext)
{
  orxOBJECT *pstObject;

  /* For all objects */
  for(pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT));
      pstObject != orxNULL;
      pstObject = orxOBJECT(orxStructure_GetNext(pstObject)))
  {
    /* Is object enabled? */
    if(orxObject_IsEnabled(pstObject) != orxFALSE)
    {
      orxU32    i;
      orxFRAME *pstFrame;

      /* Has life time? */
      if(orxStructure_TestFlags(pstObject, orxOBJECT_KU32_FLAG_HAS_LIFETIME))
      {
        /* Updates its life time */
        pstObject->fLifeTime -= _pstClockInfo->fDT;

        /* Should die? */
        if(pstObject->fLifeTime <= orxFLOAT_0)
        {
          orxOBJECT *pstDeleteObject;

          /* Stores object to delete */
          pstDeleteObject = pstObject;

          /* Reverts to previous object */
          pstObject = orxOBJECT(orxStructure_GetPrevious(pstObject));

          /* Deletes it */
          orxObject_Delete(pstDeleteObject);

          /* Is previous invalid? */
          if(pstObject == orxNULL)
          {
            pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT));
          }

          continue;
        }
      }

      /* !!! TODO !!! */
      /* Updates culling info before calling update subfunctions */

      /* For all linked structures */
      for(i = 0; i < orxSTRUCTURE_ID_LINKABLE_NUMBER; i++)
      {
        /* Is structure linked? */
        if(pstObject->astStructure[i].pstStructure != orxNULL)
        {
          /* Updates it */
          if(orxStructure_Update(pstObject->astStructure[i].pstStructure, pstObject, _pstClockInfo) == orxSTATUS_FAILURE)
          {
            /* Logs message */
            orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to update object structure.");
          }
        }
      }

      /* Has frame? */
      if((pstFrame = orxOBJECT_GET_STRUCTURE(pstObject, FRAME)) != orxNULL)
      {
        /* Has no body? */
        if(orxOBJECT_GET_STRUCTURE(pstObject, BODY) == orxNULL)
        {
          orxVECTOR vPosition, vMove;

          /* Gets its position */
          orxFrame_GetPosition(pstFrame, orxFRAME_SPACE_LOCAL, &vPosition);

          /* Computes its move */
          orxVector_Mulf(&vMove, &(pstObject->vSpeed), _pstClockInfo->fDT);

          /* Gets its new position */
          orxVector_Add(&vPosition, &vPosition, &vMove);

          /* Updates its rotation */
          orxFrame_SetRotation(pstFrame, orxFrame_GetRotation(pstFrame, orxFRAME_SPACE_LOCAL) + (pstObject->fAngularVelocity * _pstClockInfo->fDT));

          /* Stores it */
          orxFrame_SetPosition(pstFrame, &vPosition);
        }
      }
    }
  }

  return;
}

/***************************************************************************
 * Public functions                                                        *
 ***************************************************************************/

/** Object module setup
 */
orxVOID orxObject_Setup()
{
  /* Adds module dependencies */
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_MEMORY);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_BANK);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_STRUCTURE);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_FRAME);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_CLOCK);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_CONFIG);
  orxModule_AddDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_EVENT);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_GRAPHIC);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_BODY);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_ANIMPOINTER);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_FXPOINTER);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_SOUNDPOINTER);
  orxModule_AddOptionalDependency(orxMODULE_ID_OBJECT, orxMODULE_ID_SPAWNER);

  return;
}

/** Inits the object module
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxObject_Init()
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Not already Initialized? */
  if(!(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY))
  {
    /* Cleans static controller */
    orxMemory_Zero(&sstObject, sizeof(orxOBJECT_STATIC));

    /* Registers structure type */
    eResult = orxSTRUCTURE_REGISTER(OBJECT, orxSTRUCTURE_STORAGE_TYPE_LINKLIST, orxMEMORY_TYPE_MAIN, orxNULL);

    /* Initialized? */
    if(eResult == orxSTATUS_SUCCESS)
    {
      /* Creates objects clock */
      sstObject.pstClock = orxClock_FindFirst(orx2F(-1.0f), orxCLOCK_TYPE_CORE);

      /* Valid? */
      if(sstObject.pstClock != orxNULL)
      {
        /* Registers object update function to clock */
        eResult = orxClock_Register(sstObject.pstClock, orxObject_UpdateAll, orxNULL, orxMODULE_ID_OBJECT, orxCLOCK_PRIORITY_LOW);

        /* Success? */
        if(eResult == orxSTATUS_SUCCESS)
        {
          /* Inits Flags */
          sstObject.u32Flags = orxOBJECT_KU32_STATIC_FLAG_READY | orxOBJECT_KU32_STATIC_FLAG_CLOCK;
        }
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to register link list structure.");
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Tried to initialize object module when it was already initialized.");

    /* Already initialized */
    eResult = orxSTATUS_SUCCESS;
  }

  /* Done! */
  return eResult;
}

/** Exits from the object module
 */
orxVOID orxObject_Exit()
{
  /* Initialized? */
  if(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY)
  {
    /* Deletes object list */
    orxObject_DeleteAll();

    /* Has clock? */
    if(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_CLOCK)
    {
      /* Unregisters object update all function */
      orxClock_Unregister(sstObject.pstClock, orxObject_UpdateAll);

      /* Removes reference */
      sstObject.pstClock = orxNULL;

      /* Updates flags */
      sstObject.u32Flags &= ~orxOBJECT_KU32_STATIC_FLAG_CLOCK;
    }

    /* Unregisters structure type */
    orxStructure_Unregister(orxSTRUCTURE_ID_OBJECT);

    /* Updates flags */
    sstObject.u32Flags &= ~orxOBJECT_KU32_STATIC_FLAG_READY;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Tried to exit from object module when it wasn't initialized.");
  }

  return;
}

/** Creates an empty object
 * @return      Created orxOBJECT / orxNULL
 */
orxOBJECT *orxObject_Create()
{
  orxOBJECT *pstObject;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);

  /* Creates object */
  pstObject = orxOBJECT(orxStructure_Create(orxSTRUCTURE_ID_OBJECT));

  /* Created? */
  if(pstObject != orxNULL)
  {
    /* Clears its color */
    orxObject_ClearColor(pstObject);

    /* Inits flags */
    orxStructure_SetFlags(pstObject, orxOBJECT_KU32_FLAG_ENABLED, orxOBJECT_KU32_MASK_ALL);

    /* Not creating it internally? */
    if(!orxFLAG_TEST(sstObject.u32Flags, orxOBJECT_KU32_STATIC_FLAG_INTERNAL))
    {
      orxEVENT stEvent;

      /* Inits event */
      orxMemory_Zero(&stEvent, sizeof(orxEVENT));
      stEvent.eType   = orxEVENT_TYPE_OBJECT;
      stEvent.eID     = orxOBJECT_EVENT_CREATE;
      stEvent.hSender = pstObject;

      /* Sends it */
      orxEvent_Send(&stEvent);
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to create object object. hehe");
  }

  return pstObject;
}

/** Deletes an object
 * @param[in] _pstObject        Concerned object
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_Delete(orxOBJECT *_pstObject)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Not referenced? */
  if(orxStructure_GetRefCounter(_pstObject) == 0)
  {
    orxEVENT  stEvent;
    orxU32    i;

    /* Inits event */
    orxMemory_Zero(&stEvent, sizeof(orxEVENT));
    stEvent.eType   = orxEVENT_TYPE_OBJECT;
    stEvent.eID     = orxOBJECT_EVENT_DELETE;
    stEvent.hSender = _pstObject;

    /* Sends it */
    orxEvent_Send(&stEvent);

    /* Unlink all structures */
    for(i = 0; i < orxSTRUCTURE_ID_LINKABLE_NUMBER; i++)
    {
      orxObject_UnlinkStructure(_pstObject, (orxSTRUCTURE_ID)i);
    }

    /* Removes owner */
    orxObject_SetOwner(_pstObject, orxNULL);

    /* Deletes structure */
    orxStructure_Delete(_pstObject);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Tried to delete object when it was still referenced.");

    /* Referenced by others */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Creates an object from config
 * @param[in]   _zConfigID            Config ID
 * @ return orxOBJECT / orxNULL
 */
orxOBJECT *orxFASTCALL orxObject_CreateFromConfig(orxCONST orxSTRING _zConfigID)
{
  orxOBJECT  *pstResult;
  orxSTRING   zPreviousSection;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxASSERT((_zConfigID != orxNULL) && (_zConfigID != orxSTRING_EMPTY));

  /* Gets previous config section */
  zPreviousSection = orxConfig_GetCurrentSection();

  /* Selects section */
  if((orxConfig_HasSection(_zConfigID) != orxFALSE)
  && (orxConfig_SelectSection(_zConfigID) != orxSTATUS_FAILURE))
  {
    /* Sets internal flag */
    orxFLAG_SET(sstObject.u32Flags, orxOBJECT_KU32_STATIC_FLAG_INTERNAL, orxOBJECT_KU32_STATIC_FLAG_NONE);

    /* Creates object */
    pstResult = orxObject_Create();

    /* Removes internal flag */
    orxFLAG_SET(sstObject.u32Flags, orxOBJECT_KU32_STATIC_FLAG_NONE, orxOBJECT_KU32_STATIC_FLAG_INTERNAL);

    /* Valid? */
    if(pstResult != orxNULL)
    {
      orxSTRING zGraphicFileName, zAnimPointerName, zAutoScrolling, zFlipping, zBodyName, zSpawnerName, zCameraName;
      orxFRAME *pstFrame;
      orxU32    u32FrameFlags, u32Flags;
      orxVECTOR vValue, vParentSize;
      orxBOOL   bHasParent = orxFALSE;
      orxEVENT  stEvent;

      /* Defaults to 2D flags */
      u32Flags = orxOBJECT_KU32_FLAG_2D;

      /* Stores reference */
      pstResult->zReference = orxConfig_GetCurrentSection();

      /* *** Frame *** */

      /* Gets auto scrolling value */
      zAutoScrolling = orxString_LowerCase(orxConfig_GetString(orxOBJECT_KZ_CONFIG_AUTO_SCROLL));

      /* X auto scrolling? */
      if(orxString_Compare(zAutoScrolling, orxOBJECT_KZ_X) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags   = orxFRAME_KU32_FLAG_SCROLL_X;
      }
      /* Y auto scrolling? */
      else if(orxString_Compare(zAutoScrolling, orxOBJECT_KZ_Y) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags   = orxFRAME_KU32_FLAG_SCROLL_Y;
      }
      /* Both auto scrolling? */
      else if(orxString_Compare(zAutoScrolling, orxOBJECT_KZ_BOTH) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags   = orxFRAME_KU32_FLAG_SCROLL_X | orxFRAME_KU32_FLAG_SCROLL_Y;
      }
      else
      {
        /* Updates frame flags */
        u32FrameFlags   = orxFRAME_KU32_FLAG_NONE;
      }

      /* Gets flipping value */
      zFlipping = orxString_LowerCase(orxConfig_GetString(orxOBJECT_KZ_CONFIG_FLIP));

      /* X flipping? */
      if(orxString_Compare(zFlipping, orxOBJECT_KZ_X) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags  |= orxFRAME_KU32_FLAG_FLIP_X;
      }
      /* Y flipping? */
      else if(orxString_Compare(zFlipping, orxOBJECT_KZ_Y) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags  |= orxFRAME_KU32_FLAG_FLIP_Y;
      }
      /* Both flipping? */
      else if(orxString_Compare(zFlipping, orxOBJECT_KZ_BOTH) == 0)
      {
        /* Updates frame flags */
        u32FrameFlags  |= orxFRAME_KU32_FLAG_FLIP_X | orxFRAME_KU32_FLAG_FLIP_Y;
      }

      /* Depth scaling active? */
      if(orxConfig_GetBool(orxOBJECT_KZ_CONFIG_DEPTH_SCALE) != orxFALSE)
      {
        /* Updates frame flags */
        u32FrameFlags  |= orxFRAME_KU32_FLAG_DEPTH_SCALE;
      }

      /* Creates frame */
      pstFrame = orxFrame_Create(u32FrameFlags);

      /* Valid? */
      if(pstFrame != orxNULL)
      {
        /* Links it */
        if(orxObject_LinkStructure(pstResult, orxSTRUCTURE(pstFrame)) != orxSTATUS_FAILURE)
        {
          /* Updates flags */
          orxFLAG_SET(pstResult->astStructure[orxSTRUCTURE_ID_FRAME].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);
        }
      }

      /* *** Parent *** */

      /* Gets camera file name */
      zCameraName = orxConfig_GetString(orxOBJECT_KZ_CONFIG_PARENT_CAMERA);

      /* Valid? */
      if((zCameraName != orxNULL) && (zCameraName != orxSTRING_EMPTY))
      {
        orxCAMERA *pstCamera;

        /* Gets camera */
        pstCamera = orxCamera_CreateFromConfig(zCameraName);

        /* Valid? */
        if(pstCamera != orxNULL)
        {
          orxAABOX stFrustum;

          /* Sets it as parent */
          orxObject_SetParent(pstResult, pstCamera);

          /* Updates parent status */
          bHasParent = orxTRUE;

          /* Gets camera frustum */
          orxCamera_GetFrustum(pstCamera, &stFrustum);

          /* Gets parent size */
          orxVector_Sub(&vParentSize, &(stFrustum.vBR), &(stFrustum.vTL));
        }
      }

      /* *** Graphic *** */

      /* Gets graphic file name */
      zGraphicFileName = orxConfig_GetString(orxOBJECT_KZ_CONFIG_GRAPHIC_NAME);

      /* Valid? */
      if((zGraphicFileName != orxNULL) && (zGraphicFileName != orxSTRING_EMPTY))
      {
        orxGRAPHIC *pstGraphic;

        /* Creates graphic */
        pstGraphic = orxGraphic_CreateFromConfig(zGraphicFileName);

        /* Valid? */
        if(pstGraphic != orxNULL)
        {
          /* Links it structures */
          if(orxObject_LinkStructure(pstResult, orxSTRUCTURE(pstGraphic)) != orxSTATUS_FAILURE)
          {
            /* Updates flags */
            orxFLAG_SET(pstResult->astStructure[orxSTRUCTURE_ID_GRAPHIC].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);
          }
        }
      }

      /* *** Animation *** */

      /* Gets animation set name */
      zAnimPointerName = orxConfig_GetString(orxOBJECT_KZ_CONFIG_ANIMPOINTER_NAME);

      /* Valid? */
      if((zAnimPointerName != orxNULL) && (zAnimPointerName != orxSTRING_EMPTY))
      {
        orxANIMPOINTER *pstAnimPointer;

        /* Creates animation pointer from it */
        pstAnimPointer = orxAnimPointer_CreateFromConfig(orxSTRUCTURE(pstResult), zAnimPointerName);

        /* Valid? */
        if(pstAnimPointer != orxNULL)
        {
          /* Links it structures */
          if(orxObject_LinkStructure(pstResult, orxSTRUCTURE(pstAnimPointer)) != orxSTATUS_FAILURE)
          {
            /* Updates flags */
            orxFLAG_SET(pstResult->astStructure[orxSTRUCTURE_ID_ANIMPOINTER].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);

            /* Has frequency? */
            if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_FREQUENCY) != orxFALSE)
            {
              /* Updates animation pointer frequency */
              orxObject_SetAnimFrequency(pstResult, orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_FREQUENCY));
            }
          }
        }
      }

      /* Has scale? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_SCALE) != orxFALSE)
      {
        /* Is config scale not a vector? */
        if(orxConfig_GetVector(orxOBJECT_KZ_CONFIG_SCALE, &vValue) == orxNULL)
        {
          orxFLOAT fScale;

          /* Gets config uniformed scale */
          fScale = orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_SCALE);

          /* Updates vector */
          orxVector_SetAll(&vValue, fScale);
        }

        /* Use parent space and has a valid parent? */
        if((bHasParent != orxFALSE)
        && ((orxConfig_HasValue(orxOBJECT_KZ_CONFIG_USE_PARENT_SPACE) == orxFALSE)
         || (orxConfig_GetBool(orxOBJECT_KZ_CONFIG_USE_PARENT_SPACE) != orxFALSE)))
        {
          /* Gets world space values */
          orxVector_Mul(&vValue, &vValue, &vParentSize);
        }

        /* Updates object scale */
        orxObject_SetScale(pstResult, &vValue);
      }

      /* Has color? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_COLOR) != orxFALSE)
      {
        orxVECTOR vColor;

        /* Gets its value */
        orxConfig_GetVector(orxOBJECT_KZ_CONFIG_COLOR, &vColor);

        /* Applies it */
        orxColor_SetRGB(&(pstResult->stColor), &vColor);

        /* Updates status flags */
        u32Flags |= orxOBJECT_KU32_FLAG_HAS_COLOR;
      }

      /* Has alpha? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_ALPHA) != orxFALSE)
      {
        /* Applies it */
        orxColor_SetAlpha(&(pstResult->stColor), orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_ALPHA));

        /* Updates status */
        u32Flags |= orxOBJECT_KU32_FLAG_HAS_COLOR;
      }

      /* *** Body *** */

      /* Gets body name */
      zBodyName = orxConfig_GetString(orxOBJECT_KZ_CONFIG_BODY);

      /* Valid? */
      if((zBodyName != orxNULL) && (zBodyName != orxSTRING_EMPTY))
      {
        orxBODY *pstBody;

        /* Creates body */
        pstBody = orxBody_CreateFromConfig(orxSTRUCTURE(pstResult), zBodyName);

        /* Valid? */
        if(pstBody != orxNULL)
        {
          /* Links it */
          if(orxObject_LinkStructure(pstResult, orxSTRUCTURE(pstBody)) != orxSTATUS_FAILURE)
          {
            /* Updates flags */
            orxFLAG_SET(pstResult->astStructure[orxSTRUCTURE_ID_BODY].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);
          }
        }
      }

      /* *** Spawner *** */

      /* Gets spawner name */
      zSpawnerName = orxConfig_GetString(orxOBJECT_KZ_CONFIG_SPAWNER);

      /* Valid? */
      if((zSpawnerName != orxNULL) && (zSpawnerName != orxSTRING_EMPTY))
      {
        orxSPAWNER *pstSpawner;

        /* Creates spawner */
        pstSpawner = orxSpawner_CreateFromConfig(zSpawnerName);

        /* Valid? */
        if(pstSpawner != orxNULL)
        {
          /* Links it */
          if(orxObject_LinkStructure(pstResult, orxSTRUCTURE(pstSpawner)) != orxSTATUS_FAILURE)
          {
            /* Sets object as parent */
            orxSpawner_SetParent(pstSpawner, pstResult);

            /* Updates flags */
            orxFLAG_SET(pstResult->astStructure[orxSTRUCTURE_ID_SPAWNER].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);
          }
        }
      }

      /* *** Misc *** */

      /* Has a position? */
      if(orxConfig_GetVector(orxOBJECT_KZ_CONFIG_POSITION, &vValue) != orxNULL)
      {
        /* Use parent space and has a valid parent? */
        if((bHasParent != orxFALSE)
        && ((orxConfig_HasValue(orxOBJECT_KZ_CONFIG_USE_PARENT_SPACE) == orxFALSE)
         || (orxConfig_GetBool(orxOBJECT_KZ_CONFIG_USE_PARENT_SPACE) != orxFALSE)))
        {
          /* Gets world space values */
          orxVector_Mul(&vValue, &vValue, &vParentSize);
        }

        /* Updates object position */
        orxObject_SetPosition(pstResult, &vValue);
      }

      /* Updates object rotation */
      orxObject_SetRotation(pstResult, orxMATH_KF_DEG_TO_RAD * orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_ROTATION));

      /* Has speed? */
      if(orxConfig_GetVector(orxOBJECT_KZ_CONFIG_SPEED, &vValue) != orxNULL)
      {
        /* Uses relative speed? */
        if(orxConfig_GetBool(orxOBJECT_KZ_CONFIG_USE_RELATIVE_SPEED) != orxFALSE)
        {
          /* Updates object relative speed */
          orxObject_SetRelativeSpeed(pstResult, &vValue);
        }
        else
        {
          /* Updates object speed */
          orxObject_SetSpeed(pstResult, &vValue);
        }
      }

      /* Sets angular velocity? */
      orxObject_SetAngularVelocity(pstResult, orxMATH_KF_DEG_TO_RAD * orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_ANGULAR_VELOCITY));

      /* Has FX? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_FX) != orxFALSE)
      {
        /* Adds it */
        orxObject_AddFX(pstResult, orxConfig_GetString(orxOBJECT_KZ_CONFIG_FX));
      }

      /* Has sound? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_SOUND) != orxFALSE)
      {
        /* Adds it */
        orxObject_AddSound(pstResult, orxConfig_GetString(orxOBJECT_KZ_CONFIG_SOUND));
      }

      /* Has smoothing value? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_SMOOTHING) != orxFALSE)
      {
        /* Updates flags */
        u32Flags |= (orxConfig_GetBool(orxOBJECT_KZ_CONFIG_SMOOTHING) != orxFALSE) ? orxOBJECT_KU32_FLAG_SMOOTHING_ON : orxOBJECT_KU32_FLAG_SMOOTHING_OFF;
      }

      /* Has blend mode? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_BLEND_MODE) != orxFALSE)
      {
        orxSTRING zBlendMode;

        /* Gets blend mode value */
        zBlendMode = orxString_LowerCase(orxConfig_GetString(orxOBJECT_KZ_CONFIG_BLEND_MODE));

        /* alpha blend mode? */
        if(orxString_Compare(zBlendMode, orxOBJECT_KZ_ALPHA) == 0)
        {
          /* Updates flags */
          u32Flags |= orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA;
        }
        /* Multiply blend mode? */
        else if(orxString_Compare(zBlendMode, orxOBJECT_KZ_MULTIPLY) == 0)
        {
          /* Updates flags */
          u32Flags |= orxOBJECT_KU32_FLAG_BLEND_MODE_MULTIPLY;
        }
        /* Add blend mode? */
        else if(orxString_Compare(zBlendMode, orxOBJECT_KZ_ADD) == 0)
        {
          /* Updates flags */
          u32Flags |= orxOBJECT_KU32_FLAG_BLEND_MODE_ADD;
        }
      }
      else
      {
        /* Defaults to alpha */
        u32Flags |= orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA;
      }

      /* Has life time? */
      if(orxConfig_HasValue(orxOBJECT_KZ_CONFIG_LIFETIME) != orxFALSE)
      {
        /* Stores it */
        orxObject_SetLifeTime(pstResult, orxConfig_GetFloat(orxOBJECT_KZ_CONFIG_LIFETIME));
      }

      /* Updates flags */
      orxStructure_SetFlags(pstResult, u32Flags, orxOBJECT_KU32_FLAG_NONE);

      /* Inits event */
      orxMemory_Zero(&stEvent, sizeof(orxEVENT));
      stEvent.eType   = orxEVENT_TYPE_OBJECT;
      stEvent.eID     = orxOBJECT_EVENT_CREATE;
      stEvent.hSender = pstResult;

      /* Sends it */
      orxEvent_Send(&stEvent);
    }

    /* Restores previous section */
    orxConfig_SelectSection(zPreviousSection);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to find config section named %s.", _zConfigID);

    /* Updates result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

/** Links a structure to an object
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pstStructure   Structure to link
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_LinkStructure(orxOBJECT *_pstObject, orxSTRUCTURE *_pstStructure)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;
  orxSTRUCTURE_ID eStructureID;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxSTRUCTURE_ASSERT(_pstStructure);

  /* Gets structure id & offset */
  eStructureID = orxStructure_GetID(_pstStructure);

  /* Valid? */
  if(eStructureID < orxSTRUCTURE_ID_LINKABLE_NUMBER)
  {
    /* Unlink previous structure if needed */
    orxObject_UnlinkStructure(_pstObject, eStructureID);

    /* Updates structure reference counter */
    orxStructure_IncreaseCounter(_pstStructure);

    /* Links new structure to object */
    _pstObject->astStructure[eStructureID].pstStructure = _pstStructure;
    _pstObject->astStructure[eStructureID].u32Flags     = orxOBJECT_KU32_STORAGE_FLAG_NONE;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Invalid structure id(%i).", eStructureID);

    /* Wrong structure ID */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Unlinks structure from an object, given its structure ID
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _eStructureID   ID of structure to unlink
 */
orxVOID orxFASTCALL orxObject_UnlinkStructure(orxOBJECT *_pstObject, orxSTRUCTURE_ID _eStructureID)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_eStructureID < orxSTRUCTURE_ID_LINKABLE_NUMBER);

  /* Needs to be processed? */
  if(_pstObject->astStructure[_eStructureID].pstStructure != orxNULL)
  {
    orxSTRUCTURE *pstStructure;

    /* Gets referenced structure */
    pstStructure = _pstObject->astStructure[_eStructureID].pstStructure;

    /* Decreases structure reference counter */
    orxStructure_DecreaseCounter(pstStructure);

    /* Was internally handled? */
    if(orxFLAG_TEST(_pstObject->astStructure[_eStructureID].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL))
    {
      /* Depending on structure ID */
      switch(_eStructureID)
      {
        case orxSTRUCTURE_ID_FRAME:
        {
          orxFrame_Delete(orxFRAME(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_GRAPHIC:
        {
          orxGraphic_Delete(orxGRAPHIC(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_ANIMPOINTER:
        {
          orxAnimPointer_Delete(orxANIMPOINTER(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_BODY:
        {
          orxBody_Delete(orxBODY(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_FXPOINTER:
        {
          orxFXPointer_Delete(orxFXPOINTER(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_SOUNDPOINTER:
        {
          orxSoundPointer_Delete(orxSOUNDPOINTER(pstStructure));
          break;
        }

        case orxSTRUCTURE_ID_SPAWNER:
        {
          orxSpawner_Delete(orxSPAWNER(pstStructure));
          break;
        }

        default:
        {
          orxASSERT(orxFALSE && "Can't destroy this structure type directly from an object.");

          /* Logs message */
          orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Invalid parent's structure id.");
          break;
        }
      }
    }

    /* Cleans it */
    orxMemory_Zero(&(_pstObject->astStructure[_eStructureID]), sizeof(orxOBJECT_STORAGE));
  }

  return;
}


/* *** Structure accessors *** */


/** Structure used by an object get accessor, given its structure ID. Structure must then be cast correctly (see helper macro)
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _eStructureID   ID of the structure to get
 * @return orxSTRUCTURE / orxNULL
 */
orxSTRUCTURE *orxFASTCALL _orxObject_GetStructure(orxCONST orxOBJECT *_pstObject, orxSTRUCTURE_ID _eStructureID)
{
  orxSTRUCTURE *pstStructure = orxNULL;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Offset is valid? */
  if(_eStructureID < orxSTRUCTURE_ID_LINKABLE_NUMBER)
  {
    /* Gets requested structure */
    pstStructure = _pstObject->astStructure[_eStructureID].pstStructure;
  }

  /* Done ! */
  return pstStructure;
}

/** Enables/disables an object
 * @param[in]   _pstObject    Concerned object
 * @param[in]   _bEnable      enable / disable
 */
orxVOID orxFASTCALL orxObject_Enable(orxOBJECT *_pstObject, orxBOOL _bEnable)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Enable? */
  if(_bEnable != orxFALSE)
  {
    /* Updates status flags */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_ENABLED, orxOBJECT_KU32_FLAG_NONE);
  }
  else
  {
    /* Updates status flags */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_NONE, orxOBJECT_KU32_FLAG_ENABLED);
  }

  return;
}

/** Is object enabled?
 * @param[in]   _pstObject    Concerned object
 * @return      orxTRUE if enabled, orxFALSE otherwise
 */
orxBOOL orxFASTCALL orxObject_IsEnabled(orxCONST orxOBJECT *_pstObject)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Done! */
  return(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_ENABLED));
}

/** Sets render status of an object
 * @param[in]   _pstObject    Concerned object
 * @param[in]   _bRendered    Rendered or not this frame
 */
orxVOID orxFASTCALL orxObject_SetRendered(orxOBJECT *_pstObject, orxBOOL _bRendered)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Rendered? */
  if(_bRendered != orxFALSE)
  {
    /* Updates status flags */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_RENDERED, orxOBJECT_KU32_FLAG_NONE);
  }
  else
  {
    /* Updates status flags */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_NONE, orxOBJECT_KU32_FLAG_RENDERED);
  }

  return;
}

/** Is object rendered this frame?
 * @param[in]   _pstObject    Concerned object
 * @return      orxTRUE if rendered, orxFALSE otherwise
 */
orxBOOL orxFASTCALL orxObject_IsRendered(orxCONST orxOBJECT *_pstObject)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Done! */
  return(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_RENDERED));
}

/** Sets user data for an object
 * @param[in]   _pstObject    Concerned object
 * @param[in]   _pUserData    User data to store / orxNULL
 */
orxVOID orxFASTCALL orxObject_SetUserData(orxOBJECT *_pstObject, orxVOID *_pUserData)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Stores it */
  _pstObject->pUserData = _pUserData;

  return;
}

/** Gets object's user data
 * @param[in]   _pstObject    Concerned object
 * @return      Storeduser data / orxNULL
 */
orxVOID *orxFASTCALL orxObject_GetUserData(orxCONST orxOBJECT *_pstObject)
{
  orxVOID *pResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets user data */
  pResult = _pstObject->pUserData;

  /* Done! */
  return pResult;
}

/** Sets owner for an object
 * @param[in]   _pstObject    Concerned object
 * @param[in]   _pOwner       Owner to set / orxNULL
 */
orxVOID orxFASTCALL orxObject_SetOwner(orxOBJECT *_pstObject, orxVOID *_pOwner)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_pOwner == orxNULL) || (((orxSTRUCTURE *)(_pOwner))->eID ^ orxSTRUCTURE_MAGIC_TAG_ACTIVE) < orxSTRUCTURE_ID_NUMBER);

  /* Sets new owner */
  _pstObject->pstOwner = orxSTRUCTURE(_pOwner);

  return;
}

/** Gets object's owner
 * @param[in]   _pstObject    Concerned object
 * @return      Owner / orxNULL
 */
orxVOID *orxFASTCALL orxObject_GetOwner(orxOBJECT *_pstObject)
{
  orxVOID *pResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets owner */
  pResult = _pstObject->pstOwner;

  /* Done! */
  return pResult;
}

/** Flips object
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _bFlipX         Flip it on X axis
 * @param[in]   _bFlipY         Flip it on Y axis
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_Flip(orxOBJECT *_pstObject, orxBOOL _bFlipX, orxBOOL _bFlipY)
{
  orxFRAME *pstFrame;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets its frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    orxU32 u32Flags;

    /* Updates flags */
    u32Flags  = (_bFlipX != orxFALSE) ? orxFRAME_KU32_FLAG_FLIP_X : orxFRAME_KU32_FLAG_NONE;
    u32Flags |= (_bFlipY != orxFALSE) ? orxFRAME_KU32_FLAG_FLIP_Y : orxFRAME_KU32_FLAG_NONE;

    /* Updates frame */
    orxStructure_SetFlags(pstFrame, u32Flags, orxFRAME_KU32_FLAG_FLIP_X | orxFRAME_KU32_FLAG_FLIP_Y);

    /* Updates result */
    eResult = orxSTATUS_SUCCESS;
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets object pivot
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pvPivot        Object pivot
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetPivot(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvPivot)
{
  orxGRAPHIC *pstGraphic;
  orxSTATUS   eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvPivot != orxNULL);

  /* Gets graphic */
  pstGraphic = orxOBJECT_GET_STRUCTURE(_pstObject, GRAPHIC);

  /* Valid? */
  if(pstGraphic != orxNULL)
  {
    /* Sets object pivot */
    orxGraphic_SetPivot(pstGraphic, _pvPivot);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get graphic object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets object position
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pvPosition     Object position
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetPosition(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvPosition)
{
  orxFRAME *pstFrame;
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvPosition != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    orxBODY *pstBody;

    /* Sets object position */
    orxFrame_SetPosition(pstFrame, _pvPosition);

    /* Gets body */
    pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

    /* Valid? */
    if(pstBody != orxNULL)
    {
      /* Updates body position */
      orxBody_SetPosition(pstBody, _pvPosition);
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets object rotation
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _fRotation      Object rotation
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetRotation(orxOBJECT *_pstObject, orxFLOAT _fRotation)
{
  orxFRAME *pstFrame;
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    orxBODY *pstBody;

    /* Sets Object rotation */
    orxFrame_SetRotation(pstFrame, _fRotation);

    /* Gets body */
    pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

    /* Valid? */
    if(pstBody != orxNULL)
    {
      /* Updates body rotation */
      orxBody_SetRotation(pstBody, _fRotation);
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets Object scale
 * @param[in]   _pstObject      Concerned Object
 * @param[in]   _pvScale        Object scale vector
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetScale(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvScale)
{
  orxFRAME *pstFrame;
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvScale != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    orxBODY *pstBody;

    /* Sets frame scale */
    orxFrame_SetScale(pstFrame, _pvScale);

    /* Gets body */
    pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

    /* Valid? */
    if(pstBody != orxNULL)
    {
      /* Updates body scale */
      orxBody_SetScale(pstBody, _pvScale);
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Get object pivot
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvPivot        Object pivot
 * @return      orxVECTOR / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetPivot(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvPivot)
{
  orxGRAPHIC  *pstGraphic;
  orxVECTOR   *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvPivot != orxNULL);

  /* Gets graphic */
  pstGraphic = orxOBJECT_GET_STRUCTURE(_pstObject, GRAPHIC);

  /* Valid? */
  if(pstGraphic != orxNULL)
  {
    /* Gets object pivot */
     pvResult = orxGraphic_GetPivot(pstGraphic, _pvPivot);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get graphic object.");

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Get object position
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvPosition     Object position
 * @return      orxVECTOR / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetPosition(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvPosition)
{
  orxFRAME  *pstFrame;
  orxVECTOR *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvPosition != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object position */
    pvResult = orxFrame_GetPosition(pstFrame, orxFRAME_SPACE_LOCAL, _pvPosition);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Get object world position
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvPosition     Object world position
 * @return      orxVECTOR / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetWorldPosition(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvPosition)
{
  orxFRAME  *pstFrame;
  orxVECTOR *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvPosition != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object position */
    pvResult = orxFrame_GetPosition(pstFrame, orxFRAME_SPACE_GLOBAL, _pvPosition);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Get object rotation
 * @param[in]   _pstObject      Concerned object
 * @return      orxFLOAT
 */
orxFLOAT orxFASTCALL orxObject_GetRotation(orxCONST orxOBJECT *_pstObject)
{
  orxFRAME *pstFrame;
  orxFLOAT fResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object rotation */
    fResult = orxFrame_GetRotation(pstFrame, orxFRAME_SPACE_LOCAL);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    fResult = orxFLOAT_0;
  }

  /* Done! */
  return fResult;
}

/** Get object world rotation
 * @param[in]   _pstObject      Concerned object
 * @return      orxFLOAT
 */
orxFLOAT orxFASTCALL orxObject_GetWorldRotation(orxCONST orxOBJECT *_pstObject)
{
  orxFRAME *pstFrame;
  orxFLOAT fResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object rotation */
    fResult = orxFrame_GetRotation(pstFrame, orxFRAME_SPACE_GLOBAL);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Updates result */
    fResult = orxFLOAT_0;
  }

  /* Done! */
  return fResult;
}

/** Gets object scale
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvScale        Object scale vector
 * @return      Scale vector
 */
orxVECTOR *orxFASTCALL orxObject_GetScale(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvScale)
{
  orxFRAME *pstFrame;
  orxVECTOR *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvScale != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object scale */
    orxFrame_GetScale(pstFrame, orxFRAME_SPACE_LOCAL, _pvScale);

    /* Clears scale on Z */

    _pvScale->fZ = orxFLOAT_1;

    /* Updates result */
    pvResult = _pvScale;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Clears vector */
    orxVector_SetAll(_pvScale, orxFLOAT_0);

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Gets object world scale
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvScale        Object world scale
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxVECTOR *orxFASTCALL orxObject_GetWorldScale(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvScale)
{
  orxFRAME  *pstFrame;
  orxVECTOR *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvScale != orxNULL);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* Valid? */
  if(pstFrame != orxNULL)
  {
    /* Gets object scale */
    pvResult = orxFrame_GetScale(pstFrame, orxFRAME_SPACE_GLOBAL, _pvScale);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get frame object.");

    /* Clears scale */
    orxVector_Copy(_pvScale, &orxVECTOR_0);

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Sets an object parent
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pParent        Parent structure to set (object, spawner, camera or frame) / orxNULL
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetParent(orxOBJECT *_pstObject, orxVOID *_pParent)
{
  orxFRAME   *pstFrame;
  orxSTATUS   eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_pParent == orxNULL) || (((orxSTRUCTURE *)(_pParent))->eID ^ orxSTRUCTURE_MAGIC_TAG_ACTIVE) < orxSTRUCTURE_ID_NUMBER);

  /* Gets frame */
  pstFrame = orxOBJECT_GET_STRUCTURE(_pstObject, FRAME);

  /* No parent? */
  if(_pParent == orxNULL)
  {
    /* Removes parent */
    orxFrame_SetParent(pstFrame, orxNULL);
  }
  else
  {
    /* Depending on parent ID */
    switch(orxStructure_GetID(_pParent))
    {
      case orxSTRUCTURE_ID_CAMERA:
      {
        /* Updates its parent */
        orxFrame_SetParent(pstFrame, orxCamera_GetFrame(orxCAMERA(_pParent)));

        break;
      }

      case orxSTRUCTURE_ID_FRAME:
      {
        /* Updates its parent */
        orxFrame_SetParent(pstFrame, orxFRAME(_pParent));

        break;
      }

      case orxSTRUCTURE_ID_OBJECT:
      {
        /* Updates its parent */
        orxFrame_SetParent(pstFrame, orxOBJECT_GET_STRUCTURE(orxOBJECT(_pParent), FRAME));

        break;
      }

      case orxSTRUCTURE_ID_SPAWNER:
      {
        /* Updates its parent */
        orxFrame_SetParent(pstFrame, orxSpawner_GetFrame(orxSPAWNER(_pParent)));

        break;
      }

      default:
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Invalid parent's structure id.");

        /* Updates result */
        eResult = orxSTATUS_FAILURE;

        break;
      }
    }
  }

  /* Done! */
  return eResult;
}

/** Gets object size
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvSize         Object's size
 * @return      orxVECTOR / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetSize(orxCONST orxOBJECT *_pstObject, orxVECTOR *_pvSize)
{
  orxGRAPHIC *pstGraphic;
  orxVECTOR  *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvSize != orxNULL);

  /* Gets graphic */
  pstGraphic = orxOBJECT_GET_STRUCTURE(_pstObject, GRAPHIC);

  /* Valid? */
  if(pstGraphic != orxNULL)
  {
    /* Gets its size */
    pvResult = orxGraphic_GetSize(pstGraphic, _pvSize);
  }
  else
  {
    /* No size */
    orxVector_SetAll(_pvSize, orx2F(-1.0f));

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Sets an object animset
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pstAnimSet     Animation set to set / orxNULL
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetAnimSet(orxOBJECT *_pstObject, orxANIMSET *_pstAnimSet)
{
  orxANIMPOINTER *pstAnimPointer;
  orxSTATUS       eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxSTRUCTURE_ASSERT(_pstAnimSet);

  /* Creates animation pointer from animation set */
  pstAnimPointer = orxAnimPointer_Create(orxSTRUCTURE(_pstObject), _pstAnimSet);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Updates result */
    eResult = orxSTATUS_SUCCESS;

    /* Links it to the object */
    eResult = orxObject_LinkStructure(_pstObject, orxSTRUCTURE(pstAnimPointer));

    /* Success? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Updates internal flag */
      orxFLAG_SET(_pstObject->astStructure[orxSTRUCTURE_ID_ANIMSET].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);
    }
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets an object animation frequency
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _fFrequency     Frequency to set
 * @return orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetAnimFrequency(orxOBJECT *_pstObject, orxFLOAT _fFrequency)
{
  orxANIMPOINTER *pstAnimPointer;
  orxSTATUS       eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_fFrequency >= orxFLOAT_0);

  /* Gets animation pointer */
  pstAnimPointer = orxOBJECT_GET_STRUCTURE(_pstObject, ANIMPOINTER);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Updates result */
    eResult = orxAnimPointer_SetFrequency(pstAnimPointer, _fFrequency);
  }
  else
  {
    /* Updates result */
    eResult = orxFALSE;
  }

  /* Done! */
  return eResult;
}

/** Is current animation test
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zAnimName      Animation name (config's one) to test
 * @return      orxTRUE / orxFALSE
 */
orxSTATUS orxFASTCALL orxObject_IsCurrentAnim(orxOBJECT *_pstObject, orxCONST orxSTRING _zAnimName)
{
  orxANIMPOINTER *pstAnimPointer;
  orxBOOL         bResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_zAnimName != orxNULL) && (_zAnimName != orxSTRING_EMPTY));

  /* Gets animation pointer */
  pstAnimPointer = orxOBJECT_GET_STRUCTURE(_pstObject, ANIMPOINTER);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Updates result */
    bResult = (orxAnimPointer_GetCurrentAnim(pstAnimPointer) == orxString_ToCRC(_zAnimName)) ? orxTRUE : orxFALSE;
  }
  else
  {
    /* Updates result */
    bResult = orxFALSE;
  }

  /* Done! */
  return bResult;
}

/** Is target animation test
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zAnimName      Animation name (config's one) to test
 * @return      orxTRUE / orxFALSE
 */
orxSTATUS orxFASTCALL orxObject_IsTargetAnim(orxOBJECT *_pstObject, orxCONST orxSTRING _zAnimName)
{
  orxANIMPOINTER *pstAnimPointer;
  orxBOOL         bResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_zAnimName != orxNULL) && (_zAnimName != orxSTRING_EMPTY));

  /* Gets animation pointer */
  pstAnimPointer = orxOBJECT_GET_STRUCTURE(_pstObject, ANIMPOINTER);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Updates result */
    bResult = (orxAnimPointer_GetTargetAnim(pstAnimPointer) == orxString_ToCRC(_zAnimName)) ? orxTRUE : orxFALSE;
  }
  else
  {
    /* Updates result */
    bResult = orxFALSE;
  }

  /* Done! */
  return bResult;
}

/** Sets current animation for object
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zAnimName      Animation name (config's one) to set / orxNULL
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetCurrentAnim(orxOBJECT *_pstObject, orxCONST orxSTRING _zAnimName)
{
  orxANIMPOINTER *pstAnimPointer;
  orxSTATUS       eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets animation pointer */
  pstAnimPointer = orxOBJECT_GET_STRUCTURE(_pstObject, ANIMPOINTER);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Is string null or empty? */
    if((_zAnimName == orxNULL) || (_zAnimName == orxSTRING_EMPTY))
    {
      /* Resets current animation */
      eResult = orxAnimPointer_SetCurrentAnim(pstAnimPointer, orxU32_UNDEFINED);
    }
    else
    {
      /* Sets current animation */
      eResult = orxAnimPointer_SetCurrentAnim(pstAnimPointer, orxString_ToCRC(_zAnimName));
    }
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets target animation for object
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zAnimName      Animation name (config's one) to set / orxNULL
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetTargetAnim(orxOBJECT *_pstObject, orxCONST orxSTRING _zAnimName)
{
  orxANIMPOINTER *pstAnimPointer;
  orxSTATUS       eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets animation pointer */
  pstAnimPointer = orxOBJECT_GET_STRUCTURE(_pstObject, ANIMPOINTER);

  /* Valid? */
  if(pstAnimPointer != orxNULL)
  {
    /* Is string null or empty? */
    if((_zAnimName == orxNULL) || (_zAnimName == orxSTRING_EMPTY))
    {
      /* Resets target animation */
      eResult = orxAnimPointer_SetTargetAnim(pstAnimPointer, orxU32_UNDEFINED);
    }
    else
    {
      /* Sets target animation */
      eResult = orxAnimPointer_SetTargetAnim(pstAnimPointer, orxString_ToCRC(_zAnimName));
    }
  }
  else
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Sets an object speed
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pvSpeed        Speed to set
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetSpeed(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvSpeed)
{
  orxBODY  *pstBody;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvSpeed != orxNULL);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Updates its speed */
    eResult = orxBody_SetSpeed(pstBody, _pvSpeed);
  }
  else
  {
    /* Stores it */
    orxVector_Copy(&(_pstObject->vSpeed), _pvSpeed);

    /* Updates result */
    eResult = orxTRUE;
  }

  /* Done! */
  return eResult;
}

/** Sets an object speed relative to its rotation/scale
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pvSpeed        Relative speed to set
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetRelativeSpeed(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvRelativeSpeed)
{
  orxVECTOR vSpeed, vObjectScale;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvRelativeSpeed != orxNULL);

  /* Gets global speed */
  orxVector_Mul(&vSpeed, orxVector_2DRotate(&vSpeed, _pvRelativeSpeed, orxObject_GetRotation(_pstObject)), orxObject_GetScale(_pstObject, &vObjectScale));

  /* Applies it */
  eResult = orxObject_SetSpeed(_pstObject, &vSpeed);

  /* Done! */
  return eResult;
}

/** Sets an object angular velocity
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _fVelocity      Angular velocity to set
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetAngularVelocity(orxOBJECT *_pstObject, orxFLOAT _fVelocity)
{
  orxBODY  *pstBody;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Updates its angular velocity */
    eResult = orxBody_SetAngularVelocity(pstBody, _fVelocity);
  }
  else
  {
    /* Stores it */
    _pstObject->fAngularVelocity = _fVelocity;

    /* Updates result */
    eResult = orxTRUE;
  }

  /* Done! */
  return eResult;
}

/** Gets an object speed
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvSpeed        Speed to get
 * @return      Object speed / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetSpeed(orxOBJECT *_pstObject, orxVECTOR *_pvSpeed)
{
  orxBODY    *pstBody;
  orxVECTOR  *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvSpeed != orxNULL);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Gets its speed */
    pvResult = orxBody_GetSpeed(pstBody, _pvSpeed);
  }
  else
  {
    /* Stores value */
    orxVector_Copy(_pvSpeed, &(_pstObject->vSpeed));

    /* Updates result */
    pvResult = _pvSpeed;
  }

  /* Done! */
  return pvResult;
}

/** Gets an object relative speed
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvRelativeSpeed Relative speed to get
 * @return      Object relative speed / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetRelativeSpeed(orxOBJECT *_pstObject, orxVECTOR *_pvRelativeSpeed)
{
  orxVECTOR vObjectScale, *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvRelativeSpeed != orxNULL);

  /* Gets objects speed */
  pvResult = orxObject_GetSpeed(_pstObject, _pvRelativeSpeed);

  /* Valid? */
  if(pvResult != orxNULL)
  {
    /* Gets relative speed */
    orxVector_Div(pvResult, orxVector_2DRotate(pvResult, pvResult, -orxObject_GetRotation(_pstObject)), orxObject_GetScale(_pstObject, &vObjectScale));
  }

  /* Done! */
  return pvResult;
}

/** Gets an object angular velocity
 * @param[in]   _pstObject      Concerned object
 * @return      Object angular velocity
 */
orxFLOAT orxFASTCALL orxObject_GetAngularVelocity(orxOBJECT *_pstObject)
{
  orxBODY  *pstBody;
  orxFLOAT  fResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Gets its angular velocity */
    fResult = orxBody_GetAngularVelocity(pstBody);
  }
  else
  {
    /* Updates result */
    fResult = _pstObject->fAngularVelocity;
  }

  /* Done! */
  return fResult;
}

/** Gets an object center of mass
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pvMassCenter   Mass center to get
 * @return      Mass center / orxNULL
 */
orxVECTOR *orxFASTCALL orxObject_GetMassCenter(orxOBJECT *_pstObject, orxVECTOR *_pvMassCenter)
{
  orxBODY    *pstBody;
  orxVECTOR  *pvResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvMassCenter != orxNULL);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Gets its center of mass */
    pvResult = orxBody_GetMassCenter(pstBody, _pvMassCenter);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get body object.");

    /* Updates result */
    pvResult = orxNULL;
  }

  /* Done! */
  return pvResult;
}

/** Applies a torque
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _fTorque        Torque to apply
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_ApplyTorque(orxOBJECT *_pstObject, orxFLOAT _fTorque)
{
  orxBODY  *pstBody;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Applies torque */
    eResult = orxBody_ApplyTorque(pstBody, _fTorque);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get body object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Applies a force
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pvForce        Force to apply
 * @param[in]   _pvPoint        Point (world coordinates) where the force will be applied, if orxNULL, center of mass will be used
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_ApplyForce(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvForce, orxCONST orxVECTOR *_pvPoint)
{
  orxBODY  *pstBody;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvForce != orxNULL);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Applies force */
    eResult = orxBody_ApplyForce(pstBody, _pvForce, _pvPoint);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get body object.");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Applies an impulse
 * @param[in]   _pstObject        Concerned object
 * @param[in]   _pvImpulse      Impulse to apply
 * @param[in]   _pvPoint        Point (world coordinates) where the impulse will be applied, if orxNULL, center of mass will be used
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_ApplyImpulse(orxOBJECT *_pstObject, orxCONST orxVECTOR *_pvImpulse, orxCONST orxVECTOR *_pvPoint)
{
  orxBODY  *pstBody;
  orxSTATUS eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pvImpulse != orxNULL);

  /* Gets body */
  pstBody = orxOBJECT_GET_STRUCTURE(_pstObject, BODY);

  /* Valid? */
  if(pstBody != orxNULL)
  {
    /* Applies impulse */
    eResult = orxBody_ApplyImpulse(pstBody, _pvImpulse, _pvPoint);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to get body object");

    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Gets object's bounding box (OBB)
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pstBoundingBox Bounding box result
 * @return      Bounding box / orxNULL
 */
orxOBOX *orxFASTCALL orxObject_GetBoundingBox(orxCONST orxOBJECT *_pstObject, orxOBOX *_pstBoundingBox)
{
  orxVECTOR   vSize;
  orxGRAPHIC *pstGraphic;
  orxOBOX    *pstResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pstBoundingBox != orxNULL);

  /* Is 2D and has sized graphic? */
  if((orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_2D))
  && ((pstGraphic = orxOBJECT_GET_STRUCTURE(_pstObject, GRAPHIC)) != orxNULL)
  && (orxGraphic_GetSize(pstGraphic, &vSize) != orxNULL))
  {
    orxVECTOR vPivot, vPosition, vScale;
    orxFLOAT  fAngle;

    /* Gets pivot, position, scale & rotation */
    orxObject_GetPivot(_pstObject, &vPivot);
    orxObject_GetWorldPosition(_pstObject, &vPosition);
    orxObject_GetWorldScale(_pstObject, &vScale);
    fAngle = orxObject_GetWorldRotation(_pstObject);

    /* Updates pivot & size */
    orxVector_Mul(&vSize, &vSize, &vScale);
    orxVector_Mul(&vPivot, &vPivot, &vScale);

    /* Updates box */
    orxOBox_2DSet(_pstBoundingBox, &vPosition, &vPivot, &vSize, fAngle);

    /* Updates result */
    pstResult = _pstBoundingBox;
  }
  else
  {
    /* Updates result */
    pstResult = orxNULL;

    /* Cleans it */
    orxMemory_Zero(_pstBoundingBox, sizeof(orxAABOX));
  }

  /* Done! */
  return pstResult;
}

/** Sets object color
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _pstColor       Color to set
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetColor(orxOBJECT *_pstObject, orxCONST orxCOLOR *_pstColor)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pstColor != orxNULL);

  /* Stores color */
  orxColor_Copy(&(_pstObject->stColor), _pstColor);

  /* Updates its flag */
  orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_HAS_COLOR, orxOBJECT_KU32_FLAG_NONE);

  /* Done! */
  return eResult;
}

/** Clears object color
 * @param[in]   _pstObject      Concerned object
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_ClearColor(orxOBJECT *_pstObject)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Updates its flag */
  orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_NONE, orxOBJECT_KU32_FLAG_HAS_COLOR);

  /* Restores default color */
  orxColor_SetRGBA(&(_pstObject->stColor), orx2RGBA(0xFF, 0xFF, 0xFF, 0xFF));

  /* Done! */
  return eResult;
}

/** Object has color accessor
 * @param[in]   _pstObject      Concerned object
 * @return      orxTRUE / orxFALSE
 */
orxBOOL orxFASTCALL orxObject_HasColor(orxCONST orxOBJECT *_pstObject)
{
  orxBOOL bResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Updates result */
  bResult = orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_HAS_COLOR);

  /* Done! */
  return bResult;
}

/** Gets object color
 * @param[in]   _pstObject      Concerned object
 * @param[out]  _pstColor       Object's color
 * @return      orxCOLOR / orxNULL
 */
orxCOLOR *orxFASTCALL orxObject_GetColor(orxCONST orxOBJECT *_pstObject, orxCOLOR *_pstColor)
{
  orxCOLOR *pstResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT(_pstColor != orxNULL);

  /* Has color? */
  if(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_HAS_COLOR))
  {
    /* Copies color */
    orxColor_Copy(_pstColor, &(_pstObject->stColor));

    /* Updates result */
    pstResult = _pstColor;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Object does not have color.");

    /* Clears result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

/** Adds an FX using its config ID
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zFXConfigID    Config ID of the FX to add
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_AddFX(orxOBJECT *_pstObject, orxCONST orxSTRING _zFXConfigID)
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_zFXConfigID != orxNULL) && (_zFXConfigID != orxSTRING_EMPTY));

  /* Is object active? */
  if(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_ENABLED))
  {
    /* Adds FX */
    eResult = orxObject_AddDelayedFX(_pstObject, _zFXConfigID, orxFLOAT_0);
  }

  /* Done! */
  return eResult;
}

/** Adds a delayed FX using its config ID
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zFXConfigID    Config ID of the FX to add
 * @param[in]   _fDelay         Delay time
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_AddDelayedFX(orxOBJECT *_pstObject, orxCONST orxSTRING _zFXConfigID, orxFLOAT _fDelay)
{
  orxFXPOINTER *pstFXPointer;
  orxSTATUS     eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_zFXConfigID != orxNULL) && (_zFXConfigID != orxSTRING_EMPTY));
  orxASSERT(_fDelay >= orxFLOAT_0);

  /* Is object active? */
  if(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_ENABLED))
  {
    /* Gets its FXPointer */
    pstFXPointer = orxOBJECT_GET_STRUCTURE(_pstObject, FXPOINTER);

    /* Doesn't exist? */
    if(pstFXPointer == orxNULL)
    {
      /* Creates one */
      pstFXPointer = orxFXPointer_Create(orxSTRUCTURE(_pstObject));

      /* Valid? */
      if(pstFXPointer != orxNULL)
      {
        /* Links it */
        eResult = orxObject_LinkStructure(_pstObject, orxSTRUCTURE(pstFXPointer));

        /* Valid? */
        if(eResult != orxSTATUS_FAILURE)
        {
          /* Updates flags */
          orxFLAG_SET(_pstObject->astStructure[orxSTRUCTURE_ID_FXPOINTER].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);

          /* Adds FX from config */
          eResult = orxFXPointer_AddDelayedFXFromConfig(pstFXPointer, _zFXConfigID, _fDelay);
        }
      }
    }
    else
    {
      /* Adds FX from config */
      eResult = orxFXPointer_AddDelayedFXFromConfig(pstFXPointer, _zFXConfigID, _fDelay);
    }
  }

  /* Done! */
  return eResult;
}

/** Removes an FX using using its config ID
 * @param[in]   _pstObject      Concerned FXPointer
 * @param[in]   _zFXConfigID    Config ID of the FX to remove
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_RemoveFX(orxOBJECT *_pstObject, orxCONST orxSTRING _zFXConfigID)
{
  orxFXPOINTER *pstFXPointer;
  orxSTATUS     eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets its FXPointer */
  pstFXPointer = orxOBJECT_GET_STRUCTURE(_pstObject, FXPOINTER);

  /* Valid? */
  if(pstFXPointer != orxNULL)
  {
    /* Removes FX from config */
    eResult = orxFXPointer_RemoveFXFromConfig(pstFXPointer, _zFXConfigID);
  }

  /* Done! */
  return eResult;
}

/** Adds a sound using its config ID
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _zSoundConfigID Config ID of the sound to add
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_AddSound(orxOBJECT *_pstObject, orxCONST orxSTRING _zSoundConfigID)
{
  orxSOUNDPOINTER  *pstSoundPointer;
  orxSTATUS         eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);
  orxASSERT((_zSoundConfigID != orxNULL) && (_zSoundConfigID != orxSTRING_EMPTY));

  /* Is object active? */
  if(orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_ENABLED))
  {
    /* Gets its SoundPointer */
    pstSoundPointer = orxOBJECT_GET_STRUCTURE(_pstObject, SOUNDPOINTER);

    /* Doesn't exist? */
    if(pstSoundPointer == orxNULL)
    {
      /* Creates one */
      pstSoundPointer = orxSoundPointer_Create(orxSTRUCTURE(_pstObject));

      /* Valid? */
      if(pstSoundPointer != orxNULL)
      {
        /* Links it */
        eResult = orxObject_LinkStructure(_pstObject, orxSTRUCTURE(pstSoundPointer));

        /* Valid? */
        if(eResult != orxSTATUS_FAILURE)
        {
          /* Updates flags */
          orxFLAG_SET(_pstObject->astStructure[orxSTRUCTURE_ID_SOUNDPOINTER].u32Flags, orxOBJECT_KU32_STORAGE_FLAG_INTERNAL, orxOBJECT_KU32_STORAGE_MASK_ALL);

          /* Adds sound from config */
          eResult = orxSoundPointer_AddSoundFromConfig(pstSoundPointer, _zSoundConfigID);
        }
      }
    }
    else
    {
      /* Adds sound from config */
      eResult = orxSoundPointer_AddSoundFromConfig(pstSoundPointer, _zSoundConfigID);
    }
  }

  /* Done! */
  return eResult;
}

/** Removes a sound using using its config ID
 * @param[in]   _pstObject      Concerned FXPointer
 * @param[in]   _zSoundConfigID Config ID of the sound to remove
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_RemoveSound(orxOBJECT *_pstObject, orxCONST orxSTRING _zSoundConfigID)
{
  orxSOUNDPOINTER  *pstSoundPointer;
  orxSTATUS         eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets its SoundPointer */
  pstSoundPointer = orxOBJECT_GET_STRUCTURE(_pstObject, SOUNDPOINTER);

  /* Valid? */
  if(pstSoundPointer != orxNULL)
  {
    /* Removes FX from config */
    eResult = orxSoundPointer_RemoveSoundFromConfig(pstSoundPointer, _zSoundConfigID);
  }

  /* Done! */
  return eResult;
}

/** Gets last added sound (Do *NOT* destroy it directly before removing it!!!)
 * @param[in]   _pstObject      Concerned object
 * @return      orxSOUND / orxNULL
 */
orxSOUND *orxFASTCALL orxObject_GetLastAddedSound(orxCONST orxOBJECT *_pstObject)
{
  orxSOUNDPOINTER  *pstSoundPointer;
  orxSOUND         *pstResult = orxNULL;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Gets its SoundPointer */
  pstSoundPointer = orxOBJECT_GET_STRUCTURE(_pstObject, SOUNDPOINTER);

  /* Valid? */
  if(pstSoundPointer != orxNULL)
  {
    /* Updates result */
    pstResult = orxSoundPointer_GetLastAddedSound(pstSoundPointer);
  }

  /* Done! */
  return pstResult;
}

/** Gets object config name
 * @param[in]   _pstObject      Concerned object
 * @return      orxSTRING / orxSTRING_EMPTY
 */
orxSTRING orxFASTCALL orxObject_GetName(orxCONST orxOBJECT *_pstObject)
{
  orxSTRING zResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Updates result */
  zResult = (_pstObject->zReference != orxNULL) ? _pstObject->zReference : orxSTRING_EMPTY;

  /* Done! */
  return zResult;
}

/** Gets text name, if linked to one
 * @param[in]   _pstObject      Concerned object
 * @return      orxSTRING / orxSTRING_EMPTY
 */
orxSTRING orxFASTCALL orxObject_GetTextName(orxCONST orxOBJECT *_pstObject)
{
  orxTEXT    *pstText;
  orxSTRING   zResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Has text graphic? */
  if((_pstObject->astStructure[orxSTRUCTURE_ID_GRAPHIC].pstStructure != orxNULL)
  && (pstText = orxTEXT(orxGraphic_GetData(orxGRAPHIC(_pstObject->astStructure[orxSTRUCTURE_ID_GRAPHIC].pstStructure)))) != orxNULL)
  {
    /* Updates result */
    zResult = orxText_GetName(pstText);
  }
  else
  {
    /* Updates result */
    zResult = orxSTRING_EMPTY;
  }

  /* Done! */
  return zResult;
}

/** Creates a list of object at neighboring of the given box (ie. whose bounding volume intersects this box)
 * @param[in]   _pstCheckBox    Box to check intersection with
 * @return      orxBANK / orxNULL
 */
orxBANK *orxFASTCALL orxObject_CreateNeighborList(orxCONST orxOBOX *_pstCheckBox)
{
  orxOBOX    stObjectBox;
  orxOBJECT  *pstObject;
  orxBANK    *pstResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxASSERT(_pstCheckBox != orxNULL);

  /* Creates bank */
  pstResult = orxBank_Create(orxOBJECT_KU32_NEIGHBOR_LIST_SIZE, sizeof(orxOBJECT *), orxBANK_KU32_FLAG_NOT_EXPANDABLE, orxMEMORY_TYPE_TEMP);

  /* Valid? */
  if(pstResult != orxNULL)
  {
    orxU32 u32Counter;

    /* For all objects */
    for(u32Counter = 0, pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT));
        (u32Counter < orxOBJECT_KU32_NEIGHBOR_LIST_SIZE) && (pstObject != orxNULL);
        pstObject = orxOBJECT(orxStructure_GetNext(pstObject)), u32Counter++)
    {
      /* Gets its bounding box */
      if(orxObject_GetBoundingBox(pstObject, &stObjectBox) != orxNULL)
      {
        /* Is intersecting? */
        if(orxOBox_2DTestIntersection(_pstCheckBox, &stObjectBox) != orxFALSE)
        {
          orxOBJECT **ppstObject;

          /* Creates a new cell */
          ppstObject = (orxOBJECT **)orxBank_Allocate(pstResult);

          /* Valid? */
          if(ppstObject != orxNULL)
          {
            /* Adds object */
            *ppstObject = pstObject;
          }
          else
          {
            /* Logs message */
            orxDEBUG_PRINT(orxDEBUG_LEVEL_OBJECT, "Failed to allocate new cell.");
            break;
          }
        }
      }
    }
  }

  /* Done! */
  return pstResult;
}

/** Deletes an object list created with orxObject_CreateNeigborList
 * @param[in]   _astObjectList  Concerned object list
 */
orxVOID orxFASTCALL orxObject_DeleteNeighborList(orxBANK *_pstObjectList)
{
  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);

  /* Non null? */
  if(_pstObjectList != orxNULL)
  {
    /* Deletes it */
    orxBank_Delete(_pstObjectList);
  }
}

/** Sets object smoothing
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _eSmoothing     Smoothing type (enabled, default or none)
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetSmoothing(orxOBJECT *_pstObject, orxDISPLAY_SMOOTHING _eSmoothing)
{
  orxU32    u32Flags;
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Depending on smoothing type */
  switch(_eSmoothing)
  {
    case orxDISPLAY_SMOOTHING_ON:
    {
      /* Updates flags */
      u32Flags = orxOBJECT_KU32_FLAG_SMOOTHING_ON;

      break;
    }

    case orxDISPLAY_SMOOTHING_OFF:
    {
      /* Updates flags */
      u32Flags = orxOBJECT_KU32_FLAG_SMOOTHING_OFF;

      break;
    }

    default:
    case orxDISPLAY_SMOOTHING_DEFAULT:
    {
      /* Updates flags */
      u32Flags = orxOBJECT_KU32_FLAG_NONE;

      break;
    }
  }

  /* Updates status */
  orxStructure_SetFlags(_pstObject, u32Flags, orxOBJECT_KU32_FLAG_SMOOTHING_ON | orxOBJECT_KU32_FLAG_SMOOTHING_OFF);

  /* Done! */
  return eResult;
}

/** Gets object smoothing
 * @param[in]   _pstObject     Concerned object
 * @return Smoothing type (enabled, default or none)
 */
orxDISPLAY_SMOOTHING orxFASTCALL orxObject_GetSmoothing(orxCONST orxOBJECT *_pstObject)
{
  orxDISPLAY_SMOOTHING eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Updates result */
  eResult = orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_SMOOTHING_ON)
            ? orxDISPLAY_SMOOTHING_ON
            : orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_SMOOTHING_OFF)
              ? orxDISPLAY_SMOOTHING_OFF
              : orxDISPLAY_SMOOTHING_DEFAULT;

  /* Done! */
  return eResult;
}

/** Sets object blend mode
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _eBlendMode     Blend mode (alpha, multiply, add or none)
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetBlendMode(orxOBJECT *_pstObject, orxDISPLAY_BLEND_MODE _eBlendMode)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Depending on blend mode */
  switch(_eBlendMode)
  {
    case orxDISPLAY_BLEND_MODE_ALPHA:
    {
      /* Updates status */
      orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA, orxOBJECT_KU32_MASK_BLEND_MODE_ALL);

      break;
    }

    case orxDISPLAY_BLEND_MODE_MULTIPLY:
    {
      /* Updates status */
      orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_BLEND_MODE_MULTIPLY, orxOBJECT_KU32_MASK_BLEND_MODE_ALL);

      break;
    }

    case orxDISPLAY_BLEND_MODE_ADD:
    {
      /* Updates status */
      orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA, orxOBJECT_KU32_MASK_BLEND_MODE_ALL);

      break;
    }

    default:
    {
      /* Updates status */
      orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_BLEND_MODE_NONE, orxOBJECT_KU32_MASK_BLEND_MODE_ALL);

      /* Updates result */
      eResult = orxSTATUS_FAILURE;

      break;
    }
  }

  /* Done! */
  return eResult;
}

/** Gets object blend mode
 * @param[in]   _pstObject     Concerned object
 * @return Blend mode (alpha, multiply, add or none)
 */
orxDISPLAY_BLEND_MODE orxFASTCALL orxObject_GetBlendMode(orxCONST orxOBJECT *_pstObject)
{
  orxDISPLAY_BLEND_MODE eResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Depending on blend flags */
  switch(orxStructure_GetFlags(_pstObject, orxOBJECT_KU32_MASK_BLEND_MODE_ALL))
  {
    case orxOBJECT_KU32_FLAG_BLEND_MODE_ALPHA:
    {
      /* Updates result */
      eResult = orxDISPLAY_BLEND_MODE_ALPHA;

      break;
    }

    case orxOBJECT_KU32_FLAG_BLEND_MODE_MULTIPLY:
    {
      /* Updates result */
      eResult = orxDISPLAY_BLEND_MODE_MULTIPLY;

      break;
    }

    case orxOBJECT_KU32_FLAG_BLEND_MODE_ADD:
    {
      /* Updates result */
      eResult = orxDISPLAY_BLEND_MODE_ADD;

      break;
    }

    default:
    {
      /* Updates result */
      eResult = orxDISPLAY_BLEND_MODE_NONE;

      break;
    }
  }

  /* Done! */
  return eResult;
}

/** Sets object lifetime
 * @param[in]   _pstObject      Concerned object
 * @param[in]   _fLifeTime      Lifetime to set, negative value to disable it
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxObject_SetLifeTime(orxOBJECT *_pstObject, orxFLOAT _fLifeTime)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Is valid? */
  if(_fLifeTime >= orxFLOAT_0)
  {
    /* Stores it */
    _pstObject->fLifeTime = _fLifeTime;

    /* Updates status */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_HAS_LIFETIME, orxOBJECT_KU32_FLAG_NONE);
  }
  else
  {
    /* Updates status */
    orxStructure_SetFlags(_pstObject, orxOBJECT_KU32_FLAG_NONE, orxOBJECT_KU32_FLAG_HAS_LIFETIME);
  }

  /* Done! */
  return eResult;
}

/** Gets object lifetime
 * @param[in]   _pstObject      Concerned object
 * @return      Lifetime / negative value if none
 */
orxFLOAT orxFASTCALL orxObject_GetLifeTime(orxCONST orxOBJECT *_pstObject)
{
  orxFLOAT fResult;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstObject);

  /* Updates result */
  fResult = orxStructure_TestFlags(_pstObject, orxOBJECT_KU32_FLAG_HAS_LIFETIME) ? _pstObject->fLifeTime : orx2F(-1.0f);

  /* Done! */
  return fResult;
}

/** Picks the first active object with graphic "under" the given position
 * @param[in]   _pvPosition     Position to pick from
 * @return      orxOBJECT / orxNULL
 */
orxOBJECT *orxFASTCALL orxObject_Pick(orxCONST orxVECTOR *_pvPosition)
{
  orxFLOAT    fSelectedZ;
  orxOBJECT  *pstResult = orxNULL, *pstObject;

  /* Checks */
  orxASSERT(sstObject.u32Flags & orxOBJECT_KU32_STATIC_FLAG_READY);
  orxASSERT(_pvPosition != orxNULL);

  /* For all objects */
  for(pstObject = orxOBJECT(orxStructure_GetFirst(orxSTRUCTURE_ID_OBJECT)), fSelectedZ = _pvPosition->fZ;
      pstObject != orxNULL;
      pstObject = orxOBJECT(orxStructure_GetNext(pstObject)))
  {
    /* Is enabled? */
    if(orxObject_IsEnabled(pstObject) != orxFALSE)
    {
      orxGRAPHIC *pstGraphic;

      /* Has graphic? */
      if((pstGraphic = orxOBJECT_GET_STRUCTURE(pstObject, GRAPHIC)) != orxNULL)
      {
        orxVECTOR vObjectPos;

        /* Gets object position */
        orxObject_GetWorldPosition(pstObject, &vObjectPos);

        /* Is under position? */
        if(vObjectPos.fZ >= _pvPosition->fZ)
        {
          /* No selection or above it? */
          if((pstResult == orxNULL) || (vObjectPos.fZ < fSelectedZ))
          {
            orxOBOX stObjectBox;

            /* Gets its bounding box */
            if(orxObject_GetBoundingBox(pstObject, &stObjectBox) != orxNULL)
            {
              /* Is position in 2D box? */
              if(orxOBox_2DIsInside(&stObjectBox, _pvPosition) != orxFALSE)
              {
                /* Updates result */
                pstResult = pstObject;

                /* Updates selected position */
                fSelectedZ = vObjectPos.fZ;
              }
            }
          }
        }
      }
    }
  }

  /* Done! */
  return pstResult;
}
