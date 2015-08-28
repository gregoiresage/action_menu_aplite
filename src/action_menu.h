#pragma once

#include <pebble.h>
#ifdef PBL_SDK_2

//! @addtogroup ActionMenu
//! @{

struct ActionMenuItem;
typedef struct ActionMenuItem ActionMenuItem;

struct ActionMenuLevel;
typedef struct ActionMenuLevel ActionMenuLevel;

typedef enum {
  ActionMenuAlignTop = 0,
  ActionMenuAlignCenter
} ActionMenuAlign;

typedef struct ActionMenu ActionMenu;

//! Callback executed after the ActionMenu has closed, so memory may be freed.
//! @param root_level the root level passed to the ActionMenu
//! @param performed_action the ActionMenuItem for the action that was performed,
//! NULL if the ActionMenu is closing without an action being selected by the user
//! @param context the context passed to the ActionMenu
typedef void (*ActionMenuDidCloseCb)(ActionMenu *menu,
                                     const ActionMenuItem *performed_action,
                                     void *context);

//! enum value that controls whether menu items are displayed in a grid
//! (similarly to the emoji replies) or in a single column (reminiscent of \ref MenuLayer)
typedef enum {
  ActionMenuLevelDisplayModeWide, //!< Each item gets its own row
  ActionMenuLevelDisplayModeThin, //!< Grid view: multiple items per row
} ActionMenuLevelDisplayMode;

//! Callback executed when a given action is selected
//! @param action_menu the action menu currently on screen
//! @param action the action that was triggered
//! @param context the context passed to the action menu
//! @note the action menu is closed immediately after an action is performed,
//! unless it is frozen in the ActionMenuPerformActionCb
typedef void (*ActionMenuPerformActionCb)(ActionMenu *action_menu,
                                          const ActionMenuItem *action,
                                          void *context);

//! Callback invoked for each item in an action menu hierarchy.
//! @param item the current action menu item
//! @param a caller-provided context callback
typedef void (*ActionMenuEachItemCb)(const ActionMenuItem *item, void *context);

//! Configuration struct for the ActionMenu
typedef struct {
  const ActionMenuLevel *root_level; //!< the root level of the ActionMenu
  void *context; //!< a context pointer which will be accessbile when actions are performed
  struct {
    GColor background; //!< the color of the left column of the ActionMenu
    GColor foreground; //!< the color of the individual "crumbs" that indicate menu depth
  } colors;
  ActionMenuDidCloseCb will_close; //!< Called immediately before the ActionMenu closes
  ActionMenuDidCloseCb did_close; //!< a callback used to cleanup memory after the menu has closed
  ActionMenuAlign align;
} ActionMenuConfig;

//! Getter for the label of a given \ref ActionMenuItem
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the string label. NULL if invalid.
char *action_menu_item_get_label(const ActionMenuItem *item);

//! Getter for the action_data pointer of a given \ref ActionMenuitem.
//! @see action_menu_level_add_action
//! @param item the \ref ActionMenuItem of interest
//! @return a pointer to the data. NULL if invalid.
void *action_menu_item_get_action_data(const ActionMenuItem *item);

//! Create a new action menu level with storage allocated for a given number of items
//! @param num_items the number of items that will be displayed at that level
//! @note levels are freed alongside the whole hierarchy so no destroy API is provided.
//! @note by default, levels are using ActionMenuLevelDisplayModeWide.
//! Use \ref action_menu_level_set_display_mode to change it.
//! @see action_menu_hierarchy_destroy
ActionMenuLevel *action_menu_level_create(uint16_t num_items);

//! Set the action menu display mode
//! @param level The ActionMenuLevel whose display mode you want to change
//! @param display_mode The display mode for the action menu (3 vs. 1 item per row)
void action_menu_level_set_display_mode(ActionMenuLevel *level,
                                        ActionMenuLevelDisplayMode display_mode);

//! Add an action to an ActionLevel
//! @param level the level to add the action to
//! @param label the text to display for the action in the menu
//! @param cb the callback that will be triggered when this action is actuated
//! @param action_data data to pass to the callback for this action
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_action(ActionMenuLevel *level,
                                             const char *label,
                                             ActionMenuPerformActionCb cb,
                                             void *action_data);

//! Add a child to this ActionMenuLevel
//! @param level the parent level
//! @param child the child level
//! @param label the text to display in the action menu for this level
//! @return a reference to the new \ref ActionMenuItem on success, NULL if the level is full
ActionMenuItem *action_menu_level_add_child(ActionMenuLevel *level,
                                            ActionMenuLevel *child,
                                            const char *label);

//! Destroy a hierarchy of ActionMenuLevels
//! @param root the root level in the hierarchy
//! @param each_cb a callback to call on every \ref ActionMenuItem in every level
//! @param context a context pointer to pass to each_cb on invocation
//! @note Typical implementations will cleanup memory allocated for the item label/data
//!       associated with each item in the callback
//! @note Hierarchy is traversed in post-order.
//!       In other words, all children items are freed before their parent is freed.
void action_menu_hierarchy_destroy(const ActionMenuLevel *root,
                                   ActionMenuEachItemCb each_cb,
                                   void *context);

//! Get the context pointer this ActionMenu was created with
//! @param action_menu A pointer to an ActionMenu
//! @return the context pointer initially provided in the \ref ActionMenuConfig.
//! NULL if none exists.
void *action_menu_get_context(ActionMenu *action_menu);

//! Get the root level of an ActionMenu
//! @param action_menu the ActionMenu you want to know about
//! @return a pointer to the root ActionMenuLevel for the given ActionMenu, NULL if invalid
ActionMenuLevel *action_menu_get_root_level(ActionMenu *action_menu);

//! Open a new ActionMenu.
//! The ActionMenu acts much like a window. It fills the whole screen and handles clicks.
//! @param config the configuration info for this new ActionMenu
//! @return the new ActionMenu
ActionMenu *action_menu_open(ActionMenuConfig *config);

//! Freeze the ActionMenu. The ActionMenu will no longer respond to user input.
//! @note this API should be used when waiting for asynchronous operation.
//! @param action_menu the ActionMenu
void action_menu_freeze(ActionMenu *action_menu);

//! Unfreeze the ActionMenu previously frozen with \ref action_menu_freeze
//! @param action_menu the ActionMenu to unfreeze
void action_menu_unfreeze(ActionMenu *action_menu);

//! Set the result window for an ActionMenu. The result window will be
//! shown when the ActionMenu closes
//! @param action_menu the ActionMenu
//! @param result_window the window to insert, pass NULL to remove the current result window
//! @note repeated call will result in only the last call to be applied, i.e. only
//! one result window is ever set
void action_menu_set_result_window(ActionMenu *action_menu, Window *result_window);

//! Close the ActionMenu, whether it is frozen or not.
//! @note this API can be used on a frozen ActionMenu once the data required to
//! build the result window has been received and the result window has been set
//! @param action_menu the ActionMenu to close
//! @param animated whether or not show a close animation
void action_menu_close(ActionMenu *action_menu, bool animated);

//! @} // group ActionMenu

#endif