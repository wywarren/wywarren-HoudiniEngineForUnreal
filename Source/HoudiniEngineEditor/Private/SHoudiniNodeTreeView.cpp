/*
* Copyright (c) <2023> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "SHoudiniNodeTreeView.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEnginePrivatePCH.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"

// dlg?
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/ITypedTableView.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Visibility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableRow.h"

class FUICommandList;
class ITableRow;
class SWidget;
class UClass;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "HoudiniNodeTreeview"

SHoudiniNodeTreeView::~SHoudiniNodeTreeView()
{
	HoudiniRootNodeArray.Empty();
	HoudiniNetworkInfo = NULL;
}

void SHoudiniNodeTreeView::Construct(const SHoudiniNodeTreeView::FArguments& InArgs)
{
	HoudiniNetworkInfo = InArgs._HoudiniNetworkInfo;
	//Build the FHoudiniNodeInfoPtr tree data
	check(HoudiniNetworkInfo.IsValid());
	for (auto NodeInfoIt = HoudiniNetworkInfo->RootNodesInfos.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FHoudiniNodeInfoPtr NodeInfo = (*NodeInfoIt);
		//if (!NodeInfo.ParentNodeInfo.IsValid())
		if (NodeInfo->bIsRootNode)
		{
			//HoudiniRootNodeArray.Add(MakeShared<FHoudiniNodeInfo>(NodeInfo));
			HoudiniRootNodeArray.Add(NodeInfo);
		}
	}

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&HoudiniRootNodeArray)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SHoudiniNodeTreeView::OnGenerateRowHoudiniNodeTreeView)
		.OnGetChildren(this, &SHoudiniNodeTreeView::OnGetChildrenHoudiniNodeTreeView)
		.OnContextMenuOpening(this, &SHoudiniNodeTreeView::OnOpenContextMenu)
		.OnSelectionChanged(this, &SHoudiniNodeTreeView::OnSelectionChanged)
		.OnSetExpansionRecursive(this, &SHoudiniNodeTreeView::OnSetExpandRecursive)
	);
}

/** The item used for visualizing the class in the tree. */
class SHoudiniNodeTreeViewItem : public STableRow<FHoudiniNodeInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SHoudiniNodeTreeViewItem)
		: _HoudiniNodeInfo(nullptr)
		, _HoudiniNetworkInfo(nullptr)
		, _Expanded(false)
	{}

	/** The item content. */
	SLATE_ARGUMENT(FHoudiniNodeInfoPtr, HoudiniNodeInfo)
	SLATE_ARGUMENT(TSharedPtr<FHoudiniNetworkInfo>, HoudiniNetworkInfo)
	SLATE_ARGUMENT(bool, Expanded)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		HoudiniNodeInfo = InArgs._HoudiniNodeInfo;
		HoudiniNetworkInfo = InArgs._HoudiniNetworkInfo;
		
		bool bExpanded = InArgs._Expanded;

		//This is suppose to always be valid
		check(HoudiniNodeInfo.IsValid());
		check(HoudiniNetworkInfo.IsValid());

		/*
		UClass* IconClass = AActor::StaticClass();
		if (HoudiniNodeInfo->AttributeInfo.IsValid())
		{
			IconClass = HoudiniNodeInfo->AttributeInfo->GetType();
		}
		else if (FHoudiniNodeInfo->NodeType.Compare(TEXT("eLight")) == 0)
		{
			IconClass = ULightComponent::StaticClass();
			if (HoudiniNetworkInfo->LightInfo.Contains(FHoudiniNodeInfo->AttributeUniqueId))
			{
				TSharedPtr<FFbxLightInfo> LightInfo = *HoudiniNetworkInfo->LightInfo.Find(FHoudiniNodeInfo->AttributeUniqueId);
				if (LightInfo->Type == 0)
				{
					IconClass = UPointLightComponent::StaticClass();
				}
				}
				else if (LightInfo->Type == 1)
				{
					IconClass = UDirectionalLightComponent::StaticClass();
				}
				else if (LightInfo->Type == 2)
				{
					IconClass = USpotLightComponent::StaticClass();
				}
			}
		}
		else if (FHoudiniNodeInfo->NodeType.Compare(TEXT("eCamera")) == 0)
		{
			IconClass = UCameraComponent::StaticClass();
		}

		const FSlateBrush* ClassIcon = FSlateIconFinder::FindIconBrushForClass(IconClass);
		*/
		
		const FSlateBrush* ClassIcon = nullptr;
		if (HoudiniNodeInfo->NodeType.Equals("OBJ"))
			ClassIcon = bExpanded ? FAppStyle::GetBrush("Icons.FolderOpen") : FAppStyle::GetBrush("Icons.FolderClosed");
		else
			ClassIcon = FSlateIconFinder::FindIconBrushForClass(AActor::StaticClass());		

		//Prepare the tooltip
		FString Tooltip = HoudiniNodeInfo->NodeName;
		if (!HoudiniNodeInfo->NodeType.IsEmpty() && HoudiniNodeInfo->NodeType.Compare(TEXT("eNull")) != 0)
		{
			Tooltip += TEXT(" [") + HoudiniNodeInfo->NodeType + TEXT("]");
		}

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SHoudiniNodeTreeViewItem::OnItemCheckChanged)
				.IsChecked(this, &SHoudiniNodeTreeViewItem::IsItemChecked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage)
				.Image(ClassIcon)
				.Visibility(ClassIcon != FAppStyle::GetDefaultBrush() ? EVisibility::Visible : EVisibility::Collapsed)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 3.0f, 6.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(HoudiniNodeInfo->NodeName))
				.ToolTipText(FText::FromString(Tooltip))
			]

		];

		STableRow<FHoudiniNodeInfoPtr>::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
		);
	}

private:

	/*
	void RecursivelySetLODMeshImportState(TSharedPtr<FHoudiniNodeInfo> NodeInfo, bool bState)
	{
		//Set the bImportNode property for all mesh under eLODGroup
		for (TSharedPtr<FHoudiniNodeInfo> ChildNodeInfo : NodeInfo->Childrens)
		{
			if (!ChildNodeInfo.IsValid())
				continue;
			if (ChildNodeInfo->NodeType.Compare(TEXT("eMesh")) == 0)
			{
				ChildNodeInfo->bImportNode = bState;
			}
			else
			{
				RecursivelySetLODMeshImportState(ChildNodeInfo, bState);
			}
		}
	}
	*/

	void OnItemCheckChanged(ECheckBoxState CheckType)
	{
		if (!HoudiniNodeInfo.IsValid())
			return;

		HoudiniNodeInfo->bImportNode = CheckType == ECheckBoxState::Checked;

		/*
		if (HoudiniNodeInfo->NodeType.Compare(TEXT("eLODGroup")) == 0)
		{
			RecursivelySetLODMeshImportState(HoudiniNodeInfo, HoudiniNodeInfo->bImportNode);
		}
		*/
		/*
		if (FHoudiniNodeInfo->NodeType.Compare(TEXT("eMesh")) == 0)
		{
			//Verify if parent is a LOD group
			TSharedPtr<FHoudiniNodeInfo> ParentLODNodeInfo = FHoudiniNetworkInfo::RecursiveFindLODParentNode(FHoudiniNodeInfo);
			if (ParentLODNodeInfo.IsValid())
			{
				ParentLODNodeInfo->bImportNode = FHoudiniNodeInfo->bImportNode;
				RecursivelySetLODMeshImportState(ParentLODNodeInfo, FHoudiniNodeInfo->bImportNode);
			}
		}
		*/
	}

	ECheckBoxState IsItemChecked() const
	{
		return HoudiniNodeInfo->bImportNode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** The node info to build the tree view row from. */
	FHoudiniNodeInfoPtr HoudiniNodeInfo;
	TSharedPtr<FHoudiniNetworkInfo> HoudiniNetworkInfo;
};

TSharedRef<ITableRow> SHoudiniNodeTreeView::OnGenerateRowHoudiniNodeTreeView(FHoudiniNodeInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SHoudiniNodeTreeViewItem> ReturnRow = SNew(SHoudiniNodeTreeViewItem, OwnerTable)
		.HoudiniNodeInfo(Item)
		.HoudiniNetworkInfo(HoudiniNetworkInfo)
		.Expanded(IsItemExpanded(Item));
	return ReturnRow;
}

void SHoudiniNodeTreeView::OnGetChildrenHoudiniNodeTreeView(FHoudiniNodeInfoPtr InParent, TArray<FHoudiniNodeInfoPtr>& OutChildren)
{
	for (auto Child : InParent->Childrens)
	{
		OutChildren.Add(Child);
	}
}

void SHoudiniNodeTreeView::RecursiveSetImport(FHoudiniNodeInfoPtr NodeInfoPtr, bool ImportStatus)
{
	NodeInfoPtr->bImportNode = ImportStatus;
	for (auto Child : NodeInfoPtr->Childrens)
	{
		RecursiveSetImport(Child, ImportStatus);
	}
}

void SHoudiniNodeTreeView::OnToggleSelectAll(ECheckBoxState CheckType)
{
	//check all actor for import
	for (auto NodeInfoIt = HoudiniNetworkInfo->RootNodesInfos.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FHoudiniNodeInfoPtr NodeInfo = (*NodeInfoIt);
		//if (!NodeInfo.ParentNodeInfo.IsValid())
		if(NodeInfo->bIsRootNode)
		{
			RecursiveSetImport(NodeInfo, CheckType == ECheckBoxState::Checked);
		}
	}
}

void RecursiveSetExpand(SHoudiniNodeTreeView* TreeView, FHoudiniNodeInfoPtr NodeInfoPtr, bool ExpandState)
{
	TreeView->SetItemExpansion(NodeInfoPtr, ExpandState);
	for (auto Child : NodeInfoPtr->Childrens)
	{
		RecursiveSetExpand(TreeView, Child, ExpandState);
	}
}

void SHoudiniNodeTreeView::OnSetExpandRecursive(FHoudiniNodeInfoPtr NodeInfoPtr, bool ExpandState)
{
	RecursiveSetExpand(this, NodeInfoPtr, ExpandState);
}

FReply SHoudiniNodeTreeView::OnExpandAll()
{
	for (auto NodeInfoIt = HoudiniNetworkInfo->RootNodesInfos.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FHoudiniNodeInfoPtr NodeInfo = (*NodeInfoIt);
		//if (!NodeInfo.ParentNodeInfo.IsValid())
		if (NodeInfo->bIsRootNode)
		{
			RecursiveSetExpand(this, NodeInfo, true);
		}
	}
	return FReply::Handled();
}

FReply SHoudiniNodeTreeView::OnCollapseAll()
{
	for (auto NodeInfoIt = HoudiniNetworkInfo->RootNodesInfos.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FHoudiniNodeInfoPtr NodeInfo = (*NodeInfoIt);
		//if (!NodeInfo.ParentNodeInfo.IsValid())
		if (NodeInfo->bIsRootNode)
		{
			RecursiveSetExpand(this, NodeInfo, false);
		}
	}
	return FReply::Handled();
}

TSharedPtr<SWidget> SHoudiniNodeTreeView::OnOpenContextMenu()
{
	// Build up the menu for a selection
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>());

	//Get the different type of the multi selection
	TArray<FHoudiniNodeInfoPtr> SelectedHoudiniNodeInfos;
	const auto NumSelectedItems = GetSelectedItems(SelectedHoudiniNodeInfos);

	// We always create a section here, even if there is no parent so that clients can still extend the menu
	MenuBuilder.BeginSection("HoudiniSceneTreeViewContextMenuImportSection");
	{
		const FSlateIcon PlusIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus");
		MenuBuilder.AddMenuEntry(LOCTEXT("CheckForImport", "Add Selection To Import"), FText(), PlusIcon, FUIAction(FExecuteAction::CreateSP(this, &SHoudiniNodeTreeView::AddSelectionToImport)));
		const FSlateIcon MinusIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus");
		MenuBuilder.AddMenuEntry(LOCTEXT("UncheckForImport", "Remove Selection From Import"), FText(), MinusIcon, FUIAction(FExecuteAction::CreateSP(this, &SHoudiniNodeTreeView::RemoveSelectionFromImport)));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SHoudiniNodeTreeView::AddSelectionToImport()
{
	SetSelectionImportState(true);
}

void SHoudiniNodeTreeView::RemoveSelectionFromImport()
{
	SetSelectionImportState(false);
}

void SHoudiniNodeTreeView::SetSelectionImportState(bool MarkForImport)
{
	TArray<FHoudiniNodeInfoPtr> SelectedHoudiniNodeInfos;
	GetSelectedItems(SelectedHoudiniNodeInfos);
	for (auto Item : SelectedHoudiniNodeInfos)
	{
		FHoudiniNodeInfoPtr ItemPtr = Item;
		ItemPtr->bImportNode = MarkForImport;
	}
}

void SHoudiniNodeTreeView::OnSelectionChanged(FHoudiniNodeInfoPtr Item, ESelectInfo::Type SelectionType)
{
}


#undef LOCTEXT_NAMESPACE

