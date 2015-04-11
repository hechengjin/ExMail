/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsHTMLCanvasElement_h__)
#define nsHTMLCanvasElement_h__

#include "nsIDOMHTMLCanvasElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"
#include "nsNodeInfoManager.h"

#include "nsICanvasElementExternal.h"
#include "nsLayoutUtils.h"

class nsICanvasRenderingContextInternal;
class nsIDOMFile;
class nsIPropertyBag;

namespace mozilla {
namespace layers {
class CanvasLayer;
class LayerManager;
}
}

class nsHTMLCanvasElement : public nsGenericHTMLElement,
                            public nsICanvasElementExternal,
                            public nsIDOMHTMLCanvasElement
{
  typedef mozilla::layers::CanvasLayer CanvasLayer;
  typedef mozilla::layers::LayerManager LayerManager;

public:
  nsHTMLCanvasElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLCanvasElement();

  static nsHTMLCanvasElement* FromContent(nsIContent* aPossibleCanvas)
  {
    if (!aPossibleCanvas || !aPossibleCanvas->IsHTML(nsGkAtoms::canvas)) {
      return nullptr;
    }
    return static_cast<nsHTMLCanvasElement*>(aPossibleCanvas);
  }
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLCanvasElement
  NS_DECL_NSIDOMHTMLCANVASELEMENT

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLCanvasElement,
                                           nsGenericHTMLElement)

  /**
   * Get the size in pixels of this canvas element
   */
  nsIntSize GetSize();

  /**
   * Determine whether the canvas is write-only.
   */
  bool IsWriteOnly();

  /**
   * Force the canvas to be write-only.
   */
  void SetWriteOnly();

  /**
   * Notify that some canvas content has changed and the window may
   * need to be updated. aDamageRect is in canvas coordinates.
   */
  void InvalidateCanvasContent(const gfxRect* aDamageRect);
  /*
   * Notify that we need to repaint the entire canvas, including updating of
   * the layer tree.
   */
  void InvalidateCanvas();

  /*
   * Get the number of contexts in this canvas, and request a context at
   * an index.
   */
  int32_t CountContexts ();
  nsICanvasRenderingContextInternal *GetContextAtIndex (int32_t index);

  /*
   * Returns true if the canvas context content is guaranteed to be opaque
   * across its entire area.
   */
  bool GetIsOpaque();

  /*
   * nsICanvasElementExternal -- for use outside of content/layout
   */
  NS_IMETHOD_(nsIntSize) GetSizeExternal();
  NS_IMETHOD RenderContextsExternal(gfxContext *aContext,
                                    gfxPattern::GraphicsFilter aFilter,
                                    uint32_t aFlags = RenderFlagPremultAlpha);

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, int32_t aModType) const;

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  nsresult CopyInnerTo(nsGenericElement* aDest);

  /*
   * Helpers called by various users of Canvas
   */

  already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                               CanvasLayer *aOldLayer,
                                               LayerManager *aManager);
  // Should return true if the canvas layer should always be marked inactive.
  // We should return true here if we can't do accelerated compositing with
  // a non-BasicCanvasLayer.
  bool ShouldForceInactiveLayer(LayerManager *aManager);

  // Call this whenever we need future changes to the canvas
  // to trigger fresh invalidation requests. This needs to be called
  // whenever we render the canvas contents to the screen, or whenever we
  // take a snapshot of the canvas that needs to be "live" (e.g. -moz-element).
  void MarkContextClean();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  nsIntSize GetWidthHeight();

  nsresult UpdateContext(nsIPropertyBag *aNewContextOptions = nullptr);
  nsresult ExtractData(const nsAString& aType,
                       const nsAString& aOptions,
                       nsIInputStream** aStream,
                       bool& aFellBackToPNG);
  nsresult ToDataURLImpl(const nsAString& aMimeType,
                         nsIVariant* aEncoderOptions,
                         nsAString& aDataURL);
  nsresult MozGetAsFileImpl(const nsAString& aName,
                            const nsAString& aType,
                            nsIDOMFile** aResult);
  nsresult GetContextHelper(const nsAString& aContextId,
                            bool aForceThebes,
                            nsICanvasRenderingContextInternal **aContext);

  nsString mCurrentContextId;
  nsCOMPtr<nsICanvasRenderingContextInternal> mCurrentContext;
  
public:
  // Record whether this canvas should be write-only or not.
  // We set this when script paints an image from a different origin.
  // We also transitively set it when script paints a canvas which
  // is itself write-only.
  bool                     mWriteOnly;
};

inline nsISupports*
GetISupports(nsHTMLCanvasElement* p)
{
  return static_cast<nsGenericElement*>(p);
}

#endif /* nsHTMLCanvasElement_h__ */
