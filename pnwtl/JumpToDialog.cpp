/**
 * @file jumptodialog.cpp
 * @brief Jump To Dialog Implementation
 * @author Simon Steele
 * @note Copyright (c) 2004 Simon Steele <s.steele@pnotepad.org>
 *
 * Programmers Notepad 2 : The license file (license.[txt|html]) describes 
 * the conditions under which this source may be modified / distributed.
 */
#include "stdafx.h"
#include "resource.h"
#include "jumpto.h"
#include "jumptodialog.h"

#define COLWIDTH_PARENT 90
#define COLWIDTH_LINE	60
#define COLWIDTH_TAG	153

static int jumpToTagImages [TAG_MAX+1] = 
{
	0,	/* TAG_UNKNOWN */
	1,	/* TAG_FUNCTION */
	2,  /* TAG_PROCEDURE */
	3,	/* TAG_CLASS */
	4,	/* TAG_DEFINE */
	5,	/* TAG_ENUM */
	0,	/* TAG_FILENAME */
	0,  /* TAG_ENUMNAME */
	6,	/* TAG_MEMBER */
	7,	/* TAG_PROTOTYPE */
	8,	/* TAG_STRUCTURE */
	9,	/* TAG_TYPEDEF */
	0,	/* TAG_UNION */
	10	/* TAG_VARIABLE */
};

CJumpToDialog::CJumpToDialog(CChildFrame* pChild)
{
	m_pChild = pChild;
}

LRESULT CJumpToDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	list.Attach(GetDlgItem(IDC_JUMPTOLIST));

	btnOk.Attach(GetDlgItem(IDOK));
	btnCancel.Attach(GetDlgItem(IDCANCEL));
	edtTag.Attach(GetDlgItem(IDC_JUMPTOTEXT));

	list.InsertColumn(0, _T("Tag"), LVCFMT_LEFT, COLWIDTH_TAG, 0);
	list.InsertColumn(1, _T("Parent"), LVCFMT_LEFT, COLWIDTH_PARENT, 0);
	list.InsertColumn(2, _T("Line"), LVCFMT_LEFT, COLWIDTH_LINE, 0);

	images.CreateFromImage(IDB_TAGTYPES, 16, 1, RGB(255,0,255), IMAGE_BITMAP);
	list.SetImageList( images.m_hImageList, LVSIL_SMALL );

	int colOrder[3] = {1, 0, 2};
	list.SetColumnOrderArray(3, colOrder);

	list.SetExtendedListViewStyle( LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );

	CRect rc, rcList, rcBtn;
	GetClientRect(&rc);
	list.GetWindowRect(&rcList);
	ScreenToClient(&rcList);
	listGaps.cx = rc.Width() - rcList.right;
	listGaps.cy = rc.Height() - rcList.bottom;
	
	btnOk.GetWindowRect(&rcBtn);
	ScreenToClient(&rcBtn);
	buttonGap = rcBtn.left - rcList.right;

	JumpToHandler::GetInstance()->DoJumpTo(m_pChild, this);

	return 0;
}

LRESULT CJumpToDialog::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	int width = LOWORD(lParam);
	int height = HIWORD(lParam);

    CRect rc, rc2;
	
	// Size the list control
	list.GetWindowRect(&rc);
	ScreenToClient(&rc);
	rc.right = width - listGaps.cx;
	rc.bottom = height - listGaps.cy;
	list.SetWindowPos(HWND_TOP, &rc, SWP_NOMOVE | SWP_NOZORDER);

	// The edit box
	edtTag.GetWindowRect(&rc2);
	ScreenToClient(&rc2);
	rc2.right = rc.right;
	edtTag.SetWindowPos(HWND_TOP, &rc2, SWP_NOMOVE | SWP_NOZORDER);

	// Ok button
	btnOk.GetWindowRect(&rc2);
	ScreenToClient(&rc2);
	rc2.MoveToX(rc.right + buttonGap);
	btnOk.SetWindowPos(HWND_TOP, &rc2, SWP_NOSIZE | SWP_NOZORDER);

	// Cancel button
	btnCancel.GetWindowRect(&rc2);
	ScreenToClient(&rc2);
	rc2.MoveToX(rc.right + buttonGap);
	btnCancel.SetWindowPos(HWND_TOP, &rc2, SWP_NOSIZE | SWP_NOZORDER);

	// Now size the tag column to fit.
	list.GetClientRect(&rc);	
	int cw = rc.Width() - list.GetColumnWidth(1) - list.GetColumnWidth(2);
	list.SetColumnWidth(0, cw);

	return 0;
}

LRESULT CJumpToDialog::OnListDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	transfer();
	if(line != -1)
		EndDialog(IDOK);

	return 0;
}

void CJumpToDialog::OnFound(int count, LPMETHODINFO methodInfo)
{
	LVITEM lvi;
	memset(&lvi, 0, sizeof(LVITEM));
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText = (TCHAR*)methodInfo->methodName;
	if(methodInfo->type <= TAG_MAX)
		lvi.iImage = jumpToTagImages[methodInfo->type];
	else
		lvi.iImage = 0;
	int index = list.InsertItem(&lvi);

	lvi.mask = LVIF_TEXT;
	lvi.iItem = index;
	lvi.iSubItem = 1;
	lvi.pszText = (TCHAR*)methodInfo->parentName;
	list.SetItem(&lvi);

	_itot(methodInfo->lineNumber, itoabuf, 10);
	lvi.iSubItem = 2;
	lvi.pszText = itoabuf;
	list.SetItem(&lvi);

	list.SetItemData(index, methodInfo->lineNumber);
}

LRESULT CJumpToDialog::OnTextKeyPress(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CWindowText wt(GetDlgItem(IDC_JUMPTOTEXT));

	if((LPCTSTR)wt != NULL)
	{
		filter(wt);
	}

	return 0;
}

LRESULT CJumpToDialog::OnOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	transfer();
	EndDialog(wID);
	return 0;
}

LRESULT CJumpToDialog::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

int CJumpToDialog::GetLine()
{
	return line;
}

/**
 * Select the best match for what the user typed. This could potentially
 * also filter the list but that would make this dialog *much* more
 * expensive.
 */
void CJumpToDialog::filter(LPCTSTR text)
{
	LVFINDINFO lvfi;
	lvfi.flags = LVFI_PARTIAL;
	lvfi.psz = text;

	int index = list.FindItem(&lvfi, 0);
	if(index != -1)
		list.SelectItem(index);
}

/**
 * Transfer the selected item into the line member. 
 * Set to -1 if the selection isn't valid.
 */
void CJumpToDialog::transfer()
{
	int selIndex = list.GetSelectedIndex();
	if(selIndex != -1)
	{
		line = list.GetItemData(selIndex);
	}
	else
		line = -1;
}