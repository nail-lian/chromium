// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock_window_aura.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace chromeos {

LockWindow* LockWindow::Create() {
  return new LockWindowAura();
}

////////////////////////////////////////////////////////////////////////////////
// LockWindow implementation:
void LockWindowAura::Grab() {
  // We already have grab from the lock screen container, just call the ready
  // callback immediately.
  if (observer_)
    observer_->OnLockWindowReady();
}

views::Widget* LockWindowAura::GetWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// LockWindowAura private:
LockWindowAura::LockWindowAura() {
  Init();
}

LockWindowAura::~LockWindowAura() {
}

void LockWindowAura::Init() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  // TODO(flackr): Use a property to specify this container rather than
  // depending on shell implementation.
  params.parent =
      ash::Shell::GetInstance()->GetContainer(
          ash::internal::kShellWindowId_LockScreenContainer);
  views::Widget::Init(params);
  ash::SetWindowVisibilityAnimationTransition(GetNativeView(),
                                              ash::ANIMATE_NONE);
}

}  // namespace chromeos
