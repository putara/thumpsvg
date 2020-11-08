#ifndef SVG_UI_STATE_H
#define SVG_UI_STATE_H


class UIStateManager
{
private:
    class State
    {
    private:
        UINT id;
        HWND hwnd = nullptr;
        bool enabled = true;
        bool checked = false;
        UINT idRadio = 0;
        UINT idFrom = 0;
        UINT idTo = 0;
        UINT flags = 0;
        State* next = nullptr;

        static const UINT ENABLE = 1;
        static const UINT CHECK = 2;
        static const UINT RADIO = 4;

        State(UINT id, State* next)
        {
            this->id = id;
            this->next = next;
        }

    public:
        State() = delete;
        State(const State&) = delete;
        State& operator =(const State&) = delete;

        bool IsEnabled()
        {
            return this->enabled;
        }

        bool IsChecked()
        {
            if (this->hwnd != nullptr) {
                this->checked = ::IsWindowVisible(this->hwnd) != FALSE;
            }
            return this->checked;
        }

        void Enable(bool state)
        {
            if (this->IsEnabled() != state) {
                this->flags |= ENABLE;
                this->enabled = state;
            }
        }

        void Check(bool state)
        {
            if (this->IsChecked() != state) {
                this->flags |= CHECK;
                this->checked = state;
            }
            this->hwnd = nullptr;
        }

        void CheckIf(HWND hwnd)
        {
            this->flags |= CHECK;
            this->hwnd = hwnd;
        }

        void Radio(UINT id, UINT idFrom, UINT idTo)
        {
            this->flags |= RADIO;
            this->idRadio = id;
            this->idFrom = idFrom;
            this->idTo = idTo;
        }

        State* Update(UIStateManager& parent)
        {
            if (this->flags & ENABLE) {
                parent.DoEnable(this->id, this->IsEnabled());
            }
            if ((this->flags & CHECK) || this->hwnd != nullptr) {
                parent.DoCheck(this->id, this->IsChecked());
            }
            if (this->flags & RADIO) {
                parent.DoRadio(this->id, this->idFrom, this->idTo, this->id == this->idRadio);
            }
            this->flags = 0;
            return this->next;
        }

        State* Remove()
        {
            State* next = this->next;
            delete this;
            return next;
        }

        static State* Find(UINT id, State* head)
        {
            State* state = head;
            for (; state != nullptr && state->id != id; state = state->next);
            return state;
        }

        static State* Lookup(UINT id, State** head)
        {
            State* state = Find(id, *head);
            if (state == nullptr) {
                state = new (std::nothrow) State(id, *head);
                if (state == nullptr) {
                    return nullptr;
                }
                *head = state;
            }
            return state;
        }
    };

    Toolbar toolbar;
    HWND hwnd = nullptr;
    HMENU hmenu = nullptr;
    State* stateHead = nullptr;
    UINT_PTR idTimer = 0;

    void DoEnable(UINT id, bool state)
    {
        if (this->toolbar.IsNull() == false) {
            this->toolbar.Enable(id, state);
        }
        if (this->hmenu != nullptr) {
            ::EnableMenuItem(this->hmenu, id, MF_BYCOMMAND | (state ? MF_ENABLED : MF_GRAYED));
        }
    }

    void DoCheck(UINT id, bool state)
    {
        if (this->toolbar.IsNull() == false) {
            this->toolbar.Check(id, state);
        }
        if (this->hmenu != nullptr) {
            ::CheckMenuItem(this->hmenu, id, MF_BYCOMMAND | (state ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    void DoRadio(UINT id, UINT idFrom, UINT idTo, bool state)
    {
        if (this->toolbar.IsNull() == false) {
            this->toolbar.Check(id, state);
        }
        if (state) {
            ::CheckMenuRadioItem(this->hmenu, idFrom, idTo, id, MF_BYCOMMAND);
        }
    }

    void DoUpdate()
    {
        this->KillTimer();
        for (State* state = this->stateHead; state != nullptr; state = state->Update(*this));
        if (this->toolbar.IsNull() == false) {
            ::UpdateWindow(this->toolbar.GetHwnd());
        }
    }

    void SetTimer()
    {
        if (this->idTimer == 0 && this->hwnd != nullptr) {
            this->idTimer = ::SetTimer(this->hwnd, reinterpret_cast<UINT_PTR>(this), 1, [](HWND hwnd, UINT, UINT_PTR id, DWORD) -> void {
                UIStateManager* self = reinterpret_cast<UIStateManager*>(id);
                self->DoUpdate();
            });
        }
    }

    void KillTimer()
    {
        if (this->idTimer != 0) {
            ::KillTimer(this->hwnd, this->idTimer);
            this->idTimer = 0;
        }
    }

    void OnInitMenuHook(HWND, HMENU)
    {
        this->DoUpdate();
    }

    bool OnCommandHook(HWND, UINT id)
    {
        State* state = State::Find(id, this->stateHead);
        if (state != nullptr && state->IsEnabled() == false) {
            // Eat the message
            return true;
        }
        return false;
    }

public:
    UIStateManager()
    {
    }

    ~UIStateManager()
    {
        this->KillTimer();
        for (State* state = this->stateHead; state != nullptr; state = state->Remove());
    }

    UIStateManager& Attach(HWND hwnd)
    {
        this->hwnd = hwnd;
        return *this;
    }

    UIStateManager& AttachMenu(HMENU hmenu)
    {
        this->hmenu = hmenu;
        return *this;
    }

    UIStateManager& AttachMenu(HWND hwnd)
    {
        this->hmenu = ::GetMenu(hwnd);
        return *this;
    }

    UIStateManager& AttachToolbar(Toolbar toolbar)
    {
        this->toolbar.Attach(toolbar.GetHwnd());
        return *this;
    }

    UIStateManager& Enable(UINT id, bool enabled)
    {
        State* state = State::Lookup(id, &this->stateHead);
        if (state != nullptr) {
            state->Enable(enabled);
            this->SetTimer();
        }
        return *this;
    }

    UIStateManager& Check(UINT id, bool checked)
    {
        State* state = State::Lookup(id, &this->stateHead);
        if (state != nullptr) {
            state->Check(checked);
            this->SetTimer();
        }
        return *this;
    }

    UIStateManager& CheckIf(UINT id, HWND hwnd)
    {
        State* state = State::Lookup(id, &this->stateHead);
        if (state != nullptr) {
            state->CheckIf(hwnd);
            this->SetTimer();
        }
        return *this;
    }

    UIStateManager& Radio(UINT idRadio, UINT idFrom, UINT idTo)
    {
        bool updated = false;
        for (UINT id = idFrom; id <= idTo; id++) {
            State* state = State::Lookup(id, &this->stateHead);
            if (state != nullptr) {
                state->Radio(idRadio, idFrom, idTo);
                updated = true;
            }
        }
        if (updated) {
            this->SetTimer();
        }
        return *this;
    }

    bool HandleWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
    {
        *result = 0;
        switch (message) {
        case WM_INITMENU:
            OnInitMenuHook(hwnd, reinterpret_cast<HMENU>(wParam));
            break;
        case WM_COMMAND:
            return OnCommandHook(hwnd, GET_WM_COMMAND_ID(wParam, lParam));
        }
        return false;
    }
};

#endif
