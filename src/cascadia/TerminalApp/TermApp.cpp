#include "pch.h"
#include "TermApp.h"
#include <shellapi.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace ::Microsoft::Terminal::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::Microsoft::Terminal::TerminalApp::implementation
{
    TermApp::TermApp() :
        _xamlMetadataProviders{  },
        _settings{  },
        _tabs{  }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like TermApp just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is TermApp not
        // registered?" when it definitely is.
    }

    // Method Description:
    // - Load our settings, and build the UI for the terminal app. Before this
    //      method is called, it should not be assumed that the TerminalApp is
    //      usable.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::Create()
    {
        _LoadSettings();
        _Create();
    }

    TermApp::~TermApp()
    {
    }

    // Method Description:
    // - Create all of the initial UI elements of the Terminal app.
    //    * Creates the tab bar, initially hidden.
    //    * Creates the tab content area, which is where we'll display the tabs/panes.
    //    * Initializes the first terminal control, using the default profile,
    //      and adds it to our list of tabs.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_Create()
    {
        _xamlMetadataProviders.emplace_back(MUX::XamlTypeInfo::XamlControlsXamlMetaDataProvider{});

        // Register the MUX Xaml Controls resources as our application's resources.
        this->Resources(MUX::Controls::XamlControlsResources{});

        _tabView = MUX::Controls::TabView{};

        _tabView.SelectionChanged({ this, &TermApp::_OnTabSelectionChanged });
        _tabView.TabClosing({ this, &TermApp::_OnTabClosing });
        _tabView.Items().VectorChanged({ this, &TermApp::_OnTabItemsChanged });

        _root = Controls::Grid{};
        _tabRow = Controls::Grid{};
        _tabContent = Controls::Grid{};

        // Set up two columns in the tabs row - one for the tabs themselves, and
        // another for the settings button.
        auto hamburgerBtnColDef = Controls::ColumnDefinition();
        auto tabsColDef = Controls::ColumnDefinition();
        auto newTabBtnColDef = Controls::ColumnDefinition();

        hamburgerBtnColDef.Width(GridLengthHelper::Auto());
        newTabBtnColDef.Width(GridLengthHelper::Auto());

        _tabRow.ColumnDefinitions().Append(hamburgerBtnColDef);
        _tabRow.ColumnDefinitions().Append(tabsColDef);
        _tabRow.ColumnDefinitions().Append(newTabBtnColDef);

        // Set up two rows - one for the tabs, the other for the tab content,
        // the terminal panes.
        auto tabBarRowDef = Controls::RowDefinition();
        tabBarRowDef.Height(GridLengthHelper::Auto());
        _root.RowDefinitions().Append(tabBarRowDef);
        _root.RowDefinitions().Append(Controls::RowDefinition{});

        _root.Children().Append(_tabRow);
        _root.Children().Append(_tabContent);
        Controls::Grid::SetRow(_tabRow, 0);
        Controls::Grid::SetRow(_tabContent, 1);

        // The hamburger button is in the first column, so put the tabs in the second
        Controls::Grid::SetColumn(_tabView, 1);

        // Create the new tab button.
        _newTabButton = Controls::SplitButton{};
        Controls::SymbolIcon newTabIco{};
        newTabIco.Symbol(Controls::Symbol::Add);
        _newTabButton.Content(newTabIco);
        Controls::Grid::SetRow(_newTabButton, 0);
        Controls::Grid::SetColumn(_newTabButton, 2);
        _newTabButton.VerticalAlignment(VerticalAlignment::Stretch);
        _newTabButton.HorizontalAlignment(HorizontalAlignment::Left);

        // When the new tab button is clicked, open the default profile
        _newTabButton.Click([this](auto&&, auto&&){
            this->_OpenNewTab(std::nullopt);
        });

        // Populate the new tab button's flyout with entries for each profile
        _CreateNewTabFlyout();

        // Create the hamburger button. Other options, menus are nested in it's flyout
        _hamburgerButton = Controls::Button{};
        Controls::SymbolIcon ico{};
        ico.Symbol(Controls::Symbol::GlobalNavigationButton);
        _hamburgerButton.Content(ico);
        Controls::Grid::SetRow(_hamburgerButton, 0);
        Controls::Grid::SetColumn(_hamburgerButton, 0);
        _hamburgerButton.VerticalAlignment(VerticalAlignment::Stretch);
        _hamburgerButton.HorizontalAlignment(HorizontalAlignment::Right);

        auto hamburgerFlyout = Controls::MenuFlyout{};
        hamburgerFlyout.Placement(Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

        // Create each of the child menu items
        {
            // Create the settings button.
            auto settingsItem = Controls::MenuFlyoutItem{};
            settingsItem.Text(L"Settings");

            Controls::SymbolIcon ico{};
            ico.Symbol(Controls::Symbol::Setting);
            settingsItem.Icon(ico);

            settingsItem.Click({ this, &TermApp::_SettingsButtonOnClick });
            hamburgerFlyout.Items().Append(settingsItem);
        }
        {
            // Create the feedback button.
            auto feedbackFlyout = Controls::MenuFlyoutItem{};
            feedbackFlyout.Text(L"Feedback");

            Controls::FontIcon feedbackIco{};
            feedbackIco.Glyph(L"\xE939");
            feedbackIco.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            feedbackFlyout.Icon(feedbackIco);

            feedbackFlyout.Click({ this, &TermApp::_FeedbackButtonOnClick });
            hamburgerFlyout.Items().Append(feedbackFlyout);
        }
        _hamburgerButton.Flyout(hamburgerFlyout);

        _tabRow.Children().Append(_hamburgerButton);
        _tabRow.Children().Append(_tabView);
        _tabRow.Children().Append(_newTabButton);

        _tabContent.VerticalAlignment(VerticalAlignment::Stretch);
        _tabContent.HorizontalAlignment(HorizontalAlignment::Stretch);

        _OpenNewTab(std::nullopt);
    }

    // Method Description:
    // - Builds the flyout (dropdown) attached to the new tab button, and
    //   attaches it to the button. Populates the flyout with one entry per
    //   Profile, displaying the profile's name. Clicking each flyout item will
    //   open a new tab with that profile.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_CreateNewTabFlyout()
    {
        auto newTabFlyout = Controls::MenuFlyout{};
        for (int profileIndex = 0; profileIndex < _settings->GetProfiles().size(); profileIndex++)
        {
            const auto& profile = _settings->GetProfiles()[profileIndex];
            auto profileMenuItem = Controls::MenuFlyoutItem{};
            auto profileName = profile.GetName();
            winrt::hstring hName{ profileName };
            profileMenuItem.Text(hName);
            profileMenuItem.Click([this, profileIndex](auto&&, auto&&){
                this->_OpenNewTab({ profileIndex });
            });
            newTabFlyout.Items().Append(profileMenuItem);
        }
        _newTabButton.Flyout(newTabFlyout);
    }

    // Method Description:
    // - Look up the Xaml type associated with a TypeName (name + xaml metadata type)
    //      from a Xaml or Xbf file.
    // Arguments:
    // - type: the {name, xaml metadata type} tuple corresponding to a Xaml element
    // Return Value:
    // - Runtime Xaml type
    Windows::UI::Xaml::Markup::IXamlType TermApp::GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type)
    {
        for (const auto& provider : _xamlMetadataProviders)
        {
            auto xamlType = provider.GetXamlType(type);
            if (xamlType)
            {
                return xamlType;
            }
        }
        return nullptr;
    }

    // Method Description:
    // - Look up the Xaml type associated with a fully-qualified type name
    //      (likely of the format "namespace:element")
    // Arguments:
    // - fullName: the fully-qualified element name (including namespace) from a Xaml element
    // Return Value:
    // - Runtime Xaml type
    Windows::UI::Xaml::Markup::IXamlType TermApp::GetXamlType(hstring const& fullName)
    {
        for (const auto& provider : _xamlMetadataProviders)
        {
            auto xamlType = provider.GetXamlType(fullName);
            if (xamlType)
            {
                return xamlType;
            }
        }
        return nullptr;
    }

    // Method Description:
    // - Return the list of XML namespaces in which Xaml elements can be found
    // Arguments:
    // - <none>
    // Return Value:
    // - The list of XML namespaces in which Xaml elements can be found
    com_array<Windows::UI::Xaml::Markup::XmlnsDefinition> TermApp::GetXmlnsDefinitions()
    {
        std::vector<Windows::UI::Xaml::Markup::XmlnsDefinition> definitions;
        for (const auto& provider : _xamlMetadataProviders)
        {
            auto providerDefinitions = provider.GetXmlnsDefinitions();
            std::copy(
                std::make_move_iterator(providerDefinitions.begin()),
                std::make_move_iterator(providerDefinitions.end()),
                std::back_inserter(definitions));
        }
        return com_array<Windows::UI::Xaml::Markup::XmlnsDefinition>{ definitions };
    }

    // Method Description:
    // - Called when the settings button is clicked. ShellExecutes the settings
    //   file, as to open it in the default editor for .json files.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_SettingsButtonOnClick(const IInspectable&,
                                         const RoutedEventArgs&)
    {
        const auto settingsPath = CascadiaSettings::GetSettingsPath();
        ShellExecute(nullptr, L"open", settingsPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    // Method Description:
    // - Called when the feedback button is clicked. Launches the feedback hub
    //   to the list of all feedback for the Terminal app.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_FeedbackButtonOnClick(const IInspectable&,
                                         const RoutedEventArgs&)
    {
        // If you want this to go to the new feedback page automatically, use &newFeedback=true
        winrt::Windows::System::Launcher::LaunchUriAsync({ L"feedback-hub://?tabid=2&appid=Microsoft.WindowsTerminal_8wekyb3d8bbwe!App" });

    }

    // Method Description:
    // - Initialized our settings. See CascadiaSettings for more details.
    //      Additionally hooks up our callbacks for keybinding events to the
    //      keybindings object.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_LoadSettings()
    {
        _settings = CascadiaSettings::LoadAll();

        // Hook up the KeyBinding object's events to our handlers.
        // TODO: as we implement more events, hook them up here.
        // They should all be hooked up here, regardless of whether or not
        //      there's an actual keychord for them.
        auto kb = _settings->GetKeybindings();
        kb.NewTab([this]() { _OpenNewTab(std::nullopt); });
        kb.CloseTab([this]() { _CloseFocusedTab(); });
        kb.NewTabWithProfile([this](auto index) { _OpenNewTab({ index }); });
        kb.ScrollUp([this]() { _DoScroll(-1); });
        kb.ScrollDown([this]() { _DoScroll(1); });
        kb.NextTab([this]() { _SelectNextTab(true); });
        kb.PrevTab([this]() { _SelectNextTab(false); });
    }

    UIElement TermApp::GetRoot()
    {
        return _root;
    }

    void TermApp::_SetFocusedTabIndex(int tabIndex)
    {
        auto tab = _tabs.at(tabIndex);
        _tabView.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [tab, this](){
            auto tabViewItem = tab->GetTabViewItem();
            _tabView.SelectedItem(tabViewItem);
        });
    }

    // Method Description:
    // - Handle changes in tab layout.
    void TermApp::_UpdateTabView()
    {
        // Show tabs when there's more than 1, or the user has chosen to always
        // show the tab bar.
        const bool isVisible = (_tabs.size() > 1) ||
                               _settings->GlobalSettings().GetAlwaysShowTabs();

        // collapse/show the tabs themselves
        _tabView.Visibility(isVisible ? Visibility::Visible : Visibility::Collapsed);

        // collapse/show the row that the tabs are in.
        // NaN is the special value XAML uses for "Auto" sizing.
        _tabRow.Height(isVisible ? NAN : 0);
    }

    // Method Description:
    // - Open a new tab. This will create the TerminalControl hosting the
    //      terminal, and add a new Tab to our list of tabs. The method can
    //      optionally be provided a profile index, which will be used to create
    //      a tab using the profile in that index.
    //      If no index is provided, the default profile will be used.
    // Arguments:
    // - profileIndex: an optional index into the list of profiles to use to
    //      initialize this tab up with.
    // Return Value:
    // - <none>
    void TermApp::_OpenNewTab(std::optional<int> profileIndex)
    {
        TerminalSettings settings;

        if (profileIndex)
        {
            const auto realIndex = profileIndex.value();
            const auto profiles = _settings->GetProfiles();

            // If we don't have that many profiles, then do nothing.
            if (realIndex >= profiles.size())
            {
                return;
            }

            const auto& selectedProfile = profiles[realIndex];
            GUID profileGuid = selectedProfile.GetGuid();
            settings = _settings->MakeSettings(profileGuid);
        }
        else
        {
            // Create a tab using the default profile
            settings = _settings->MakeSettings(std::nullopt);
        }

        _CreateNewTabFromSettings(settings);
    }

    // Method Description:
    // - Creates a new tab with the given settings. If the tab bar is not being
    //      currently displayed, it will be shown.
    // Arguments:
    // - settings: the TerminalSettings object to use to create the TerminalControl with.
    // Return Value:
    // - <none>
    void TermApp::_CreateNewTabFromSettings(TerminalSettings settings)
    {
        // Initialize the new tab
        TermControl term{ settings };

        // Add the new tab to the list of our tabs.
        auto newTab = _tabs.emplace_back(std::make_shared<Tab>(term));

        auto tabViewItem = newTab->GetTabViewItem();
        _tabView.Items().Append(tabViewItem);

        // This is one way to set the tab's selected background color.
        //   tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), a Brush?);

        // This kicks off TabView::SelectionChanged, in response to which we'll attach the terminal's
        // Xaml control to the Xaml root.
        _tabView.SelectedItem(tabViewItem);
    }

    // Method Description:
    // - Returns the index in our list of tabs of the currently focused tab. If
    //      no tab is currently selected, returns -1.
    // Arguments:
    // - <none>
    // Return Value:
    // - the index of the currently focused tab if there is one, else -1
    int TermApp::_GetFocusedTabIndex() const
    {
        return _tabView.SelectedIndex();
    }

    // Method Description:
    // - Close the currently focused tab. Focus will move to the left, if possible.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_CloseFocusedTab()
    {
        if (_tabs.size() > 1)
        {
            int focusedTabIndex = _GetFocusedTabIndex();
            std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };

            // We're not calling _FocusTab here because it makes an async dispatch
            // that is practically guaranteed to not happen before we delete the tab.
            _tabView.SelectedIndex((focusedTabIndex > 0) ? focusedTabIndex - 1 : 1);
            _tabView.Items().RemoveAt(focusedTabIndex);
            _tabs.erase(_tabs.begin() + focusedTabIndex);
        }
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a number of lines. Negative values of `delta` will move the
    //      view up, and positive values will move the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    // Return Value:
    // - <none>
    void TermApp::_DoScroll(int delta)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->Scroll(delta);
    }

    // Method Description:
    // - Sets focus to the tab to the right or left the currently selected tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermApp::_SelectNextTab(const bool bMoveRight)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        auto tabCount = _tabs.size();
        // Wraparound math. By adding tabCount and then calculating modulo tabCount,
        // we clamp the values to the range [0, tabCount) while still supporting moving
        // leftward from 0 to tabCount - 1.
        _SetFocusedTabIndex(
            (tabCount + focusedTabIndex + (bMoveRight ? 1 : -1)) % tabCount
        );
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus.)
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TermApp::_OnTabSelectionChanged(const IInspectable& sender, const Controls::SelectionChangedEventArgs& eventArgs)
    {
        auto tabView = sender.as<MUX::Controls::TabView>();
        auto selectedIndex = tabView.SelectedIndex();
        if (selectedIndex >= 0)
        {
            try
            {
                auto tab = _tabs.at(selectedIndex);
                auto control = tab->GetTerminalControl().GetControl();

                _tabContent.Children().Clear();
                _tabContent.Children().Append(control);

                control.Focus(FocusState::Programmatic);
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Responds to the TabView control's Tab Closing event by removing
    //      the indicated tab from the set and focusing another one.
    //      The event is cancelled so TermApp maintains control over the
    //      items in the tabview.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TermApp::_OnTabClosing(const IInspectable& sender, const MUX::Controls::TabViewTabClosingEventArgs& eventArgs)
    {
        // Don't allow the user to close the last tab ..
        // .. yet.
        if (_tabs.size() > 1)
        {
            const auto tabViewItem = eventArgs.Item();
            uint32_t tabIndexFromControl = 0;
            _tabView.Items().IndexOf(tabViewItem, tabIndexFromControl);

            if (tabIndexFromControl == _GetFocusedTabIndex())
            {
                _tabView.SelectedIndex((tabIndexFromControl > 0) ? tabIndexFromControl - 1 : 1);
            }

            // Removing the tab from the collection will destroy its control and disconnect its connection.
            _tabs.erase(_tabs.begin() + tabIndexFromControl);
            _tabView.Items().RemoveAt(tabIndexFromControl);
        }
        // If we don't cancel the event, the TabView will remove the item itself.
        eventArgs.Cancel(true);
    }

    // Method Description:
    // - Responds to changes in the TabView's item list by changing the tabview's
    //      visibility.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TermApp::_OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs)
    {
        _UpdateTabView();
    }
}