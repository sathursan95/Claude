# Crafting System for Unreal Engine 5

A data-driven, server-authoritative crafting workflow packaged as an Unreal Engine plugin
(`Plugins/CraftingSystem`). It covers items, recipes, an inventory, a timed crafting queue and
crafting stations. Everything can be driven from Blueprint and works in multiplayer.

Requires **Unreal Engine 5.3 or later** (it uses the 5.3 data validation API).

## The workflow

```
 Designer                              Runtime (server-authoritative)
 ────────                              ──────────────────────────────
 Item Definition assets ──┐
                          ├─► Recipe assets ─► CraftingComponent.RequestCraft(Recipe, Count)
 Gameplay Tags ───────────┘                     │
                                                ├─ validate: known? station tags? in range? queue room? ingredients?
                                                ├─ consume ingredients for the whole batch
                                                ├─ craft each unit over CraftDuration / station speed
                                                ├─ deliver outputs to the inventory
                                                │    └─ no room → wait until space frees up
                                                └─ CancelCraft → refund undelivered units
                                                     └─ no room → OnItemsOverflowed (drop in world)
```

| Piece | Type | Role |
|---|---|---|
| `UCraftingItemDefinition` | Primary Data Asset | Name, icon, max stack size, tags |
| `UCraftingRecipe` | Primary Data Asset | Ingredients → outputs, craft time, required station tags |
| `UCraftingInventoryComponent` | Actor Component | Slot-based stacking inventory, replicated |
| `UCraftingComponent` | Actor Component | Known recipes, crafting queue, request validation, RPCs |
| `UCraftingStationComponent` | Actor Component | Makes an actor a workbench/forge/etc. (tags, speed, range) |

## 1. Install

1. Copy `Plugins/CraftingSystem` into your project's `Plugins/` folder.
2. Regenerate project files and build (or open the `.uproject` and let the editor build it).
3. Enable **Crafting System** under *Edit → Plugins* if it is not already on.

The plugin ships example Gameplay Tags in `Config/Tags/CraftingSystemTags.ini`
(`Station.Workbench`, `Station.Forge`, `Recipe.Category.Tools`, …). Add your own in
*Project Settings → Gameplay Tags*.

### Optional: register the asset types with the Asset Manager

Needed only if you look recipes up by `PrimaryAssetId` (for example to restore unlocked recipes
from a save game) or load them asynchronously. Add to `Config/DefaultGame.ini`:

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="CraftingItem",AssetBaseClass="/Script/CraftingSystem.CraftingItemDefinition",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/Crafting/Items")),Rules=(CookRule=AlwaysCook))
+PrimaryAssetTypesToScan=(PrimaryAssetType="CraftingRecipe",AssetBaseClass="/Script/CraftingSystem.CraftingRecipe",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/Crafting/Recipes")),Rules=(CookRule=AlwaysCook))
```

## 2. Author content

Suggested layout: `/Game/Crafting/Items` and `/Game/Crafting/Recipes`.

**Items**: *Content Browser → Add → Miscellaneous → Data Asset → CraftingItemDefinition*.
Name them `DA_Item_Wood`, `DA_Item_Plank`, etc. Set `DisplayName`, `Icon` and `MaxStackSize`
(1 for items that do not stack, such as tools).

**Recipes**: *Data Asset → CraftingRecipe*, named `DA_Recipe_Plank`, etc.

| Field | Example |
|---|---|
| Ingredients | `DA_Item_Wood ×2` |
| Outputs | `DA_Item_Plank ×4` |
| CraftDuration | `3.0` (seconds; `0` = instant) |
| RequiredStationTags | `Station.Workbench` (empty = craftable by hand) |
| RecipeTags | `Recipe.Category.Materials` (for UI tabs/filters) |

Both asset types run data validation on save and under *Tools → Validate Data*. A recipe with no
outputs, an empty item slot, a zero quantity or a negative duration is flagged as an error, so
broken recipes are caught before they ship.

## 3. Set up actors

**Player**: add to the player's **Character/Pawn** (or PlayerState/Controller):

- `CraftingInventory` (`UCraftingInventoryComponent`): set `MaxSlots` (0 = unlimited).
  You can also pre-fill `Slots` with starting items.
- `Crafting` (`UCraftingComponent`): fill `DefaultKnownRecipes`. Leave `bRequireKnownRecipes`
  on if recipes are unlocked through play.

The crafting component finds the inventory on the same actor at `BeginPlay`. If the inventory lives
elsewhere (for example on the PlayerState while crafting is on the Pawn), call `SetInventory` on the
server.

> For multiplayer, both components must be on an actor **owned by the player's PlayerController**
> (Pawn, PlayerState or Controller). Otherwise the server drops the client's requests.

**Stations**: on a workbench actor (set *Replicates* on), add `UCraftingStationComponent`:

- `StationTags`: e.g. `Station.Workbench`. A recipe is allowed when the station has **all** of
  the recipe's `RequiredStationTags`. Tags match hierarchically, so a station tagged
  `Station.Forge.Tier2` also satisfies recipes that require `Station.Forge`.
- `CraftSpeedMultiplier`: `2.0` halves craft time. It is captured when a craft is queued.
- `InteractionRange`: max distance (cm) between the player's pawn and the station.

## 4. Wire up gameplay (Blueprint)

**Using a station** (in your interact logic):

```
OnInteract(Station Actor)
  → Get Component By Class (CraftingStationComponent)
  → Crafting.SetActiveStation(Station)
  → Open crafting UI
On close UI / walk away
  → Crafting.SetActiveStation(None)
```

**Crafting UI**:

| Need | Call / Event |
|---|---|
| Recipe list for this station | `GetAvailableRecipes()` (refresh on `OnKnownRecipesChanged`) |
| Grey out unaffordable recipes | `CanCraft(Recipe, Count)` → `ECraftingResult` |
| "Craft max" slider | `GetMaxCraftable(Recipe)` |
| Craft button | `RequestCraft(Recipe, Count)` |
| Queue display | `GetQueue()` (refresh on `OnQueueChanged`) |
| Progress bar | `GetUnitProgress(Entry)` each tick or on a short timer |
| Cancel button | `CancelCraft(Entry.Id)` |
| Toasts / sounds | `OnUnitCompleted`, `OnRequestRejected` |
| Inventory grid | `CraftingInventory.GetSlots()` (refresh on `OnInventoryChanged`) |

`RequestCraft` returns the local pre-check right away, so the UI can react without waiting for
the network. The server always re-validates; if it disagrees, `OnRequestRejected` fires on the
owning client.

**Server-side hooks**:

- `UnlockRecipe(Recipe)`: call from quests, skill trees, recipe scrolls or loot.
- `OnItemsOverflowed(Items)`: fires when a cancel refunds more than fits. Spawn pickups at the
  player's feet so nothing is lost.
- `OnUnitCompleted`: also fires on the server; use it for XP, achievements or analytics.

## Rules the system enforces

- **Server authority**: all inventory and queue changes run on the server. Clients send RPCs,
  and any request is re-validated. Batch size is capped by `MaxBatchSize`.
- **No duplication**: ingredients for the whole batch are taken atomically when the craft is
  queued. A cancel refunds only units whose outputs have not been delivered. Queue state is
  committed before outputs are added, so even event handlers that cancel mid-delivery cannot
  get a unit refunded and delivered.
- **No item loss**: if outputs do not fit, the queue pauses (`bWaitingForSpace`) and resumes as
  soon as the inventory changes. Refunds that do not fit go to `OnItemsOverflowed`.
- **Atomic inventory operations**: `AddItems` and `RemoveItems` either fully succeed or change
  nothing, and duplicate entries in a request are summed before checking.
- **Replication**: the queue, known recipes and active station replicate to the owner only.
  The inventory replicates to everyone, so shared containers work. Progress bars use
  server world time, so clients show the same progress as the server.

## Extending

- **Save/load**: persist `GetSlots()` (item `PrimaryAssetId` + quantity) and `GetKnownRecipes()`.
  On load, resolve the ids through the Asset Manager, then call `AddItem` and `UnlockRecipe`.
  Register the asset types first (see above).
- **Station inventories**: put a `UCraftingInventoryComponent` on the station and call
  `SetInventory` when the player starts using it to craft from shared storage.
- **Quality/random outputs**: add a chance or quality field to the recipe outputs and roll it
  in `UCraftingComponent::DeliverHeadUnit` (`CraftingComponent.cpp`), the one place outputs
  enter the inventory.

## Tests

Automation tests live in `Source/CraftingSystem/Private/Tests`. Run them in the editor under
*Tools → Session Frontend → Automation* (filter `CraftingSystem`), or headless:

```bash
UnrealEditor-Cmd YourProject.uproject -ExecCmds="Automation RunTests CraftingSystem; Quit" -unattended -nullrhi
```

## Layout

```
Plugins/CraftingSystem/
├── CraftingSystem.uplugin
├── Config/Tags/CraftingSystemTags.ini
└── Source/CraftingSystem/
    ├── CraftingSystem.Build.cs
    ├── Public/
    │   ├── CraftingTypes.h                 FCraftingItemStack, FCraftingQueueEntry, ECraftingResult
    │   ├── CraftingItemDefinition.h
    │   ├── CraftingRecipe.h
    │   ├── CraftingInventoryComponent.h
    │   ├── CraftingComponent.h
    │   ├── CraftingStationComponent.h
    │   └── CraftingSystemModule.h
    └── Private/
        ├── *.cpp
        └── Tests/CraftingSystemTests.cpp
```
