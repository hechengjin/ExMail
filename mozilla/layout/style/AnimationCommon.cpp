/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"
#include "AnimationCommon.h"
#include "nsRuleData.h"
#include "nsCSSValue.h"
#include "nsStyleContext.h"
#include "nsIFrame.h"
#include "nsAnimationManager.h"
#include "nsLayoutUtils.h"

namespace mozilla {
namespace css {

/* static */ bool
IsGeometricProperty(nsCSSProperty aProperty)
{
  switch (aProperty) {
    case eCSSProperty_bottom:
    case eCSSProperty_height:
    case eCSSProperty_left:
    case eCSSProperty_right:
    case eCSSProperty_top:
    case eCSSProperty_width:
      return true;
    default:
      return false;
  }
}

CommonAnimationManager::CommonAnimationManager(nsPresContext *aPresContext)
  : mPresContext(aPresContext)
{
  PR_INIT_CLIST(&mElementData);
}

CommonAnimationManager::~CommonAnimationManager()
{
  NS_ABORT_IF_FALSE(!mPresContext, "Disconnect should have been called");
}

void
CommonAnimationManager::Disconnect()
{
  // Content nodes might outlive the transition or animation manager.
  RemoveAllElementData();

  mPresContext = nullptr;
}

void
CommonAnimationManager::AddElementData(CommonElementAnimationData* aData)
{
  if (PR_CLIST_IS_EMPTY(&mElementData)) {
    // We need to observe the refresh driver.
    nsRefreshDriver *rd = mPresContext->RefreshDriver();
    rd->AddRefreshObserver(this, Flush_Style);
  }

  PR_INSERT_BEFORE(aData, &mElementData);
}

void
CommonAnimationManager::ElementDataRemoved()
{
  // If we have no transitions or animations left, remove ourselves from
  // the refresh driver.
  if (PR_CLIST_IS_EMPTY(&mElementData)) {
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
}

void
CommonAnimationManager::RemoveAllElementData()
{
  while (!PR_CLIST_IS_EMPTY(&mElementData)) {
    CommonElementAnimationData *head =
      static_cast<CommonElementAnimationData*>(PR_LIST_HEAD(&mElementData));
    head->Destroy();
  }
}

/*
 * nsISupports implementation
 */

NS_IMPL_ISUPPORTS1(CommonAnimationManager, nsIStyleRuleProcessor)

nsRestyleHint
CommonAnimationManager::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

bool
CommonAnimationManager::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

nsRestyleHint
CommonAnimationManager::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
CommonAnimationManager::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ size_t
CommonAnimationManager::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mElementData
  //
  // The following members are not measured
  // - mPresContext, because it's non-owning

  return 0;
}

/* virtual */ size_t
CommonAnimationManager::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

/* static */ bool
CommonAnimationManager::ExtractComputedValueForTransition(
                          nsCSSProperty aProperty,
                          nsStyleContext* aStyleContext,
                          nsStyleAnimation::Value& aComputedValue)
{
  bool result =
    nsStyleAnimation::ExtractComputedValue(aProperty, aStyleContext,
                                           aComputedValue);
  if (aProperty == eCSSProperty_visibility) {
    NS_ABORT_IF_FALSE(aComputedValue.GetUnit() ==
                        nsStyleAnimation::eUnit_Enumerated,
                      "unexpected unit");
    aComputedValue.SetIntValue(aComputedValue.GetIntValue(),
                               nsStyleAnimation::eUnit_Visibility);
  }
  return result;
}

NS_IMPL_ISUPPORTS1(AnimValuesStyleRule, nsIStyleRule)

/* virtual */ void
AnimValuesStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  nsStyleContext *contextParent = aRuleData->mStyleContext->GetParent();
  if (contextParent && contextParent->HasPseudoElementData()) {
    // Don't apply transitions or animations to things inside of
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return;
  }

  for (uint32_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
    PropertyValuePair &cv = mPropertyValuePairs[i];
    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[cv.mProperty]))
    {
      nsCSSValue *prop = aRuleData->ValueFor(cv.mProperty);
      if (prop->GetUnit() == eCSSUnit_Null) {
#ifdef DEBUG
        bool ok =
#endif
          nsStyleAnimation::UncomputeValue(cv.mProperty,
                                           aRuleData->mPresContext,
                                           cv.mValue, *prop);
        NS_ABORT_IF_FALSE(ok, "could not store computed value");
      }
    }
  }
}

#ifdef DEBUG
/* virtual */ void
AnimValuesStyleRule::List(FILE* out, int32_t aIndent) const
{
  // WRITE ME?
}
#endif

void
ComputedTimingFunction::Init(const nsTimingFunction &aFunction)
{
  mType = aFunction.mType;
  if (mType == nsTimingFunction::Function) {
    mTimingFunction.Init(aFunction.mFunc.mX1, aFunction.mFunc.mY1,
                         aFunction.mFunc.mX2, aFunction.mFunc.mY2);
  } else {
    mSteps = aFunction.mSteps;
  }
}

static inline double
StepEnd(uint32_t aSteps, double aPortion)
{
  NS_ABORT_IF_FALSE(0.0 <= aPortion && aPortion <= 1.0, "out of range");
  uint32_t step = uint32_t(aPortion * aSteps); // floor
  return double(step) / double(aSteps);
}

double
ComputedTimingFunction::GetValue(double aPortion) const
{
  switch (mType) {
    case nsTimingFunction::Function:
      return mTimingFunction.GetSplineValue(aPortion);
    case nsTimingFunction::StepStart:
      // There are diagrams in the spec that seem to suggest this check
      // and the bounds point should not be symmetric with StepEnd, but
      // should actually step up at rather than immediately after the
      // fraction points.  However, we rely on rounding negative values
      // up to zero, so we can't do that.  And it's not clear the spec
      // really meant it.
      return 1.0 - StepEnd(mSteps, 1.0 - aPortion);
    default:
      NS_ABORT_IF_FALSE(false, "bad type");
      // fall through
    case nsTimingFunction::StepEnd:
      return StepEnd(mSteps, aPortion);
  }
}

bool
CommonElementAnimationData::CanAnimatePropertyOnCompositor(const dom::Element *aElement,
                                                           nsCSSProperty aProperty,
                                                           bool aHasGeometricProperties)
{
  bool shouldLog = nsLayoutUtils::IsAnimationLoggingEnabled();
  if (shouldLog && !gfxPlatform::OffMainThreadCompositingEnabled()) {
    nsCString message;
    message.AppendLiteral("Performance warning: Compositor disabled");
    LogAsyncAnimationFailure(message);
    return false;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (IsGeometricProperty(aProperty)) {
    if (shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation of geometric property '");
      message.Append(aProperty);
      message.AppendLiteral(" is disabled");
      LogAsyncAnimationFailure(message, aElement);
    }
    return false;
  }
  if (aProperty == eCSSProperty_opacity) {
    bool enabled = nsLayoutUtils::AreOpacityAnimationsEnabled();
    if (!enabled && shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation of 'opacity' is disabled");
      LogAsyncAnimationFailure(message);
    }
    return enabled;
  }
  if (aProperty == eCSSProperty_transform) {
    if (frame->Preserves3D() &&
        frame->Preserves3DChildren()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async animation of 'preserve-3d' transforms is not supported.  See bug 779598");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (frame->IsSVGTransformed()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async 'transform' animations of frames with SVG transforms is not supported.  See bug 779599");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (aHasGeometricProperties) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Performance warning: Async animation of 'transform' not possible due to presence of geometric properties");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    bool enabled = nsLayoutUtils::AreTransformAnimationsEnabled();
    if (!enabled && shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation of 'transform' is disabled");
      LogAsyncAnimationFailure(message);
    }
    return enabled;
  }
  return true;
}

/* static */ void
CommonElementAnimationData::LogAsyncAnimationFailure(nsCString& aMessage,
                                                     const nsIContent* aContent)
{
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->Tag()));

    nsIAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.AppendLiteral("'");
    }
    aMessage.AppendLiteral("]");
  }
  printf_stderr(aMessage.get());
}

}
}
