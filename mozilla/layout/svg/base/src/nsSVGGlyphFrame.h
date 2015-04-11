/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGLYPHFRAME_H__
#define __NS_SVGGLYPHFRAME_H__

#include "gfxFont.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsISVGChildFrame.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGUtils.h"
#include "nsTextFragment.h"

class CharacterIterator;
class gfxContext;
class nsDisplaySVGGlyphs;
class nsIDOMSVGRect;
class nsRenderingContext;
class nsSVGGlyphFrame;
class nsSVGTextFrame;
class nsSVGTextPathFrame;

struct CharacterPosition;

typedef gfxFont::DrawMode DrawMode;

typedef nsSVGGeometryFrame nsSVGGlyphFrameBase;

class nsSVGGlyphFrame : public nsSVGGlyphFrameBase,
                        public nsISVGGlyphFragmentNode,
                        public nsISVGChildFrame
{
  class AutoCanvasTMForMarker;
  friend class AutoCanvasTMForMarker;
  friend class CharacterIterator;
  friend nsIFrame*
  NS_NewSVGGlyphFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGGlyphFrame(nsStyleContext* aContext)
    : nsSVGGlyphFrameBase(aContext),
      mTextRun(nullptr),
      mStartIndex(0),
      mGetCanvasTMForFlag(nsISVGChildFrame::FOR_OUTERSVG_TM),
      mCompressWhitespace(true),
      mTrimLeadingWhitespace(false),
      mTrimTrailingWhitespace(false)
      {}
  ~nsSVGGlyphFrame()
  {
    ClearTextRun();
  }

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // These do not use the global transform if NS_STATE_NONDISPLAY_CHILD
  nsresult GetStartPositionOfChar(uint32_t charnum, nsIDOMSVGPoint **_retval);
  nsresult GetEndPositionOfChar(uint32_t charnum, nsIDOMSVGPoint **_retval);
  nsresult GetExtentOfChar(uint32_t charnum, nsIDOMSVGRect **_retval);
  nsresult GetRotationOfChar(uint32_t charnum, float *_retval);
  /**
   * @param aForceGlobalTransform controls whether to use the
   * global transform even when NS_STATE_NONDISPLAY_CHILD
   */
  float GetAdvance(bool aForceGlobalTransform);

  void SetGlyphPosition(gfxPoint *aPosition, bool aForceGlobalTransform);
  nsSVGTextPathFrame* FindTextPathParent();
  bool IsStartOfChunk(); // == is new absolutely positioned chunk.

  void GetXY(mozilla::SVGUserUnitList *aX, mozilla::SVGUserUnitList *aY);
  void SetStartIndex(uint32_t aStartIndex);
  /*
   * Returns inherited x and y values instead of parent element's attribute
   * values.
   */
  void GetEffectiveXY(int32_t strLength,
                      nsTArray<float> &aX, nsTArray<float> &aY);
  /*
   * Returns inherited dx and dy values instead of parent element's attribute
   * values.
   */
  void GetEffectiveDxDy(int32_t strLength, 
                        nsTArray<float> &aDx,
                        nsTArray<float> &aDy);
  /*
   * Returns inherited rotate values instead of parent element's attribute
   * values.
   */
  void GetEffectiveRotate(int32_t strLength,
                          nsTArray<float> &aRotate);
  uint16_t GetTextAnchor();
  bool IsAbsolutelyPositioned();
  bool IsTextEmpty() const {
    return mContent->GetText()->GetLength() == 0;
  }
  void SetTrimLeadingWhitespace(bool aTrimLeadingWhitespace) {
    if (mTrimLeadingWhitespace != aTrimLeadingWhitespace) {
      mTrimLeadingWhitespace = aTrimLeadingWhitespace;
      ClearTextRun();
    }
  }
  void SetTrimTrailingWhitespace(bool aTrimTrailingWhitespace) {
    if (mTrimTrailingWhitespace != aTrimTrailingWhitespace) {
      mTrimTrailingWhitespace = aTrimTrailingWhitespace;
      ClearTextRun();
    }
  }
  bool EndsWithWhitespace() const;
  bool IsAllWhitespace() const;

  // nsIFrame interface:
  NS_IMETHOD  CharacterDataChanged(CharacterDataChangeInfo* aInfo);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_IMETHOD  IsSelectable(bool* aIsSelectable, uint8_t* aSelectStyle) const;

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgGlyphFrame
   */
  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary

    return nsSVGGlyphFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGlyph"), aResult);
  }
#endif

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  // nsISVGChildFrame interface:
  // These four always use the global transform, even if NS_STATE_NONDISPLAY_CHILD
  NS_IMETHOD PaintSVG(nsRenderingContext *aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      uint32_t aFlags);

  NS_IMETHOD_(nsRect) GetCoveredRegion();
  virtual void ReflowSVG();
  virtual void NotifySVGChanged(uint32_t aFlags);
  NS_IMETHOD_(bool) IsDisplayContainer() { return false; }

  // nsSVGGeometryFrame methods
  gfxMatrix GetCanvasTM(uint32_t aFor);

  // nsISVGGlyphFragmentNode interface:
  // These do not use the global transform if NS_STATE_NONDISPLAY_CHILD
  virtual uint32_t GetNumberOfChars();
  virtual float GetComputedTextLength();
  virtual float GetSubStringLength(uint32_t charnum, uint32_t fragmentChars);
  virtual int32_t GetCharNumAtPosition(nsIDOMSVGPoint *point);
  NS_IMETHOD_(nsSVGGlyphFrame *) GetFirstGlyphFrame();
  NS_IMETHOD_(nsSVGGlyphFrame *) GetNextGlyphFrame();
  NS_IMETHOD_(void) SetWhitespaceCompression(bool aCompressWhitespace) {
    if (mCompressWhitespace != aCompressWhitespace) {
      mCompressWhitespace = aCompressWhitespace;
      ClearTextRun();
    }
  }

private:

  /**
   * This class exists purely because it would be too messy to pass the "for"
   * flag for GetCanvasTM through the call chains to the GetCanvasTM() call in
   * EnsureTextRun.
   */
  class AutoCanvasTMForMarker {
  public:
    AutoCanvasTMForMarker(nsSVGGlyphFrame *aFrame, uint32_t aFor)
      : mFrame(aFrame)
    {
      mOldFor = mFrame->mGetCanvasTMForFlag;
      mFrame->mGetCanvasTMForFlag = aFor;
    }
    ~AutoCanvasTMForMarker()
    {
      // Default
      mFrame->mGetCanvasTMForFlag = mOldFor;
    }
  private:
    nsSVGGlyphFrame *mFrame;
    uint32_t mOldFor;
  };

  // Use a power of 2 here. It's not so important to match
  // nsDeviceContext::AppUnitsPerDevPixel, but since we do a lot of
  // multiplying by 1/GetTextRunUnitsFactor, it's good for it to be a
  // power of 2 to avoid accuracy loss.
  static uint32_t GetTextRunUnitsFactor() { return 64; }
  
  /**
   * @aParam aDrawScale font drawing must be scaled into user units
   * by this factor
   * @param aMetricsScale font metrics must be scaled into user units
   * by this factor
   * @param aForceGlobalTransform set to true if we should force use of
   * the global transform; otherwise we won't use the global transform
   * if we're a NONDISPLAY_CHILD
   */
  bool EnsureTextRun(float *aDrawScale, float *aMetricsScale,
                       bool aForceGlobalTransform);
  void ClearTextRun();

  bool GetCharacterData(nsAString & aCharacterData);
  bool GetCharacterPositions(nsTArray<CharacterPosition>* aCharacterPositions,
                               float aMetricsScale);
  uint32_t GetTextRunFlags(uint32_t strLength);

  void AddCharactersToPath(CharacterIterator *aIter,
                           gfxContext *aContext);
  void AddBoundingBoxesToPath(CharacterIterator *aIter,
                              gfxContext *aContext);
  void DrawCharacters(CharacterIterator *aIter,
                      gfxContext *aContext,
                      DrawMode aDrawMode,
                      gfxPattern *aStrokePattern = nullptr);

  void NotifyGlyphMetricsChange();
  void SetupGlobalTransform(gfxContext *aContext, uint32_t aFor);
  nsresult GetHighlight(uint32_t *charnum, uint32_t *nchars,
                        nscolor *foreground, nscolor *background);
  float GetSubStringAdvance(uint32_t charnum, uint32_t fragmentChars,
                            float aMetricsScale);
  gfxFloat GetBaselineOffset(float aMetricsScale);

  virtual void GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy);
  virtual const SVGNumberList *GetRotate();

  // Used to support GetBBoxContribution by making GetConvasTM use this as the
  // parent transform instead of the real CanvasTM.
  nsAutoPtr<gfxMatrix> mOverrideCanvasTM;

  // Owning pointer, must call gfxTextRunWordCache::RemoveTextRun before deleting
  gfxTextRun *mTextRun;
  gfxPoint mPosition;
  // The start index into the position and rotation data
  uint32_t mStartIndex;
  uint32_t mGetCanvasTMForFlag;
  bool mCompressWhitespace;
  bool mTrimLeadingWhitespace;
  bool mTrimTrailingWhitespace;

private:
  DrawMode SetupCairoState(gfxContext *aContext,
                           gfxPattern **aStrokePattern);
};

#endif
