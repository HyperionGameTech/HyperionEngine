//! Write a Hyperion engine game in Rust.
//!
//! Implement [`Game`], then expose it two ways from the same crate:
//!
//! ```ignore
//! // lib.rs, built as a cdylib: lets the editor load the game for Play
//! hyperion::game_module!(MyGame);
//!
//! // main.rs: the standalone game
//! fn main() { std::process::exit(hyperion::run::<MyGame>()); }
//! ```
//!
//! [`engine`] has a wrapper type for every engine class, generated from the engine's reflection data, so the scene
//! is reachable without `unsafe`:
//!
//! ```ignore
//! fn on_launch(&mut self, context: &mut GameContext) {
//!     if let Some(world) = context.world() {
//!         log::info(&format!("{:?}", world.get_game_state_mode()));
//!     }
//! }
//! ```
//!
//! Methods that take or return a struct Rust has no layout for are not wrapped; they stay reachable through the raw
//! table in [`sys::bindings`].

pub use hyperion_sys as sys;
pub use hyperion_sys::{Color, Name, StringHash, Vec2f, Vec2i, Vec2u, Vec3f, Vec4f};

pub mod object;

pub use object::{EngineObject, Owned};

/// A wrapper type per engine class. Method names are the engine's, in snake case (`Node::GetLocalTranslation` is
/// `Node::get_local_translation`). A derived type dereferences to its base, so an `Entity` has every `Node` method.
#[allow(clippy::all, non_snake_case, unused_mut, unused_imports)]
pub mod engine {
    use crate::object::{take_array, take_string, to_c_string, EngineObject, Owned};
    use crate::sys;
    use crate::{Color, Name, StringHash, Vec2f, Vec2i, Vec2u, Vec3f, Vec4f};

    include!(concat!(env!("CARGO_MANIFEST_DIR"), "/../../Generated/Rust/wrappers.rs"));
}

/// Looks up (and registers) an engine [`Name`].
pub fn name(string: &str) -> Name {
    let string = object::to_c_string(string);
    let mut result = Name::default();

    unsafe { sys::Name_FromString(string.as_ptr(), false, &mut result) };

    result
}

use std::ffi::{c_char, c_int, c_void, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};

pub mod log {
    use super::*;

    fn write(level: c_int, message: &str) {
        // interior NULs can't cross as a C string
        let message = CString::new(message.replace('\0', " ")).unwrap_or_default();

        unsafe { sys::Hyp_Log(level, message.as_ptr()) }
    }

    pub fn info(message: &str) {
        write(sys::HYP_LOG_INFO, message);
    }

    pub fn warning(message: &str) {
        write(sys::HYP_LOG_WARNING, message);
    }

    pub fn error(message: &str) {
        write(sys::HYP_LOG_ERROR, message);
    }
}

/// The engine-side game object, valid for the duration of the hook it is passed to.
pub struct GameContext {
    raw: *mut sys::Game,
}

impl GameContext {
    /// The engine object, for use with [`sys::bindings`].
    pub fn raw(&self) -> *mut sys::Game {
        self.raw
    }

    /// The engine's game object.
    pub fn game(&self) -> engine::Game {
        // the engine only calls a hook with its own, live game object
        unsafe { engine::Game::from_ptr(self.raw) }.expect("game hook called without a game object")
    }

    /// Starts the world simulating (physics, scripts). Call from [`Game::on_launch`].
    pub fn start_simulating(&mut self) {
        self.game().start_simulating();
    }

    pub fn pause_simulation(&mut self) {
        self.game().pause_simulation();
    }

    /// The world the game is running.
    pub fn world(&self) -> Option<engine::World> {
        self.game().get_world()
    }
}

/// A game. Hooks run on the engine's sim thread.
pub trait Game: 'static {
    fn new() -> Self
    where
        Self: Sized;

    /// The world is ready.
    fn on_launch(&mut self, _context: &mut GameContext) {}

    /// Every tick after launch.
    fn on_update(&mut self, _context: &mut GameContext, _delta: f32) {}

    /// Before the world is torn down.
    fn on_shutdown(&mut self, _context: &mut GameContext) {}
}

fn guard(hook: &str, run: impl FnOnce()) {
    // a panic must not unwind into the engine
    if catch_unwind(AssertUnwindSafe(run)).is_err() {
        log::error(&format!("Game hook {hook} panicked"));
    }
}

unsafe extern "C" fn on_launch<G: Game>(user_data: *mut c_void, game: *mut sys::Game) {
    let state = unsafe { &mut *(user_data as *mut G) };

    guard("on_launch", || state.on_launch(&mut GameContext { raw: game }));
}

unsafe extern "C" fn on_update<G: Game>(user_data: *mut c_void, game: *mut sys::Game, delta: f32) {
    let state = unsafe { &mut *(user_data as *mut G) };

    guard("on_update", || state.on_update(&mut GameContext { raw: game }, delta));
}

unsafe extern "C" fn on_shutdown<G: Game>(user_data: *mut c_void, game: *mut sys::Game) {
    let state = unsafe { &mut *(user_data as *mut G) };

    guard("on_shutdown", || state.on_shutdown(&mut GameContext { raw: game }));
}

/// Creates the engine game object for `G`. The caller owns the returned reference.
///
/// The Rust state lives as long as the process: the engine may still call a hook after shutdown.
pub fn create_game<G: Game>() -> *mut sys::Game {
    if !sys::is_binding_abi_compatible() {
        log::error("This game was built against a different version of the engine's bindings; rebuild it");

        return std::ptr::null_mut();
    }

    let state = Box::into_raw(Box::new(G::new()));

    let callbacks = sys::HypGameCallbacks {
        on_launch: Some(on_launch::<G>),
        on_update: Some(on_update::<G>),
        on_shutdown: Some(on_shutdown::<G>),
    };

    unsafe { sys::Hyp_CreateCallbackGame(&callbacks, state as *mut c_void) }
}

/// Runs `G` as a standalone game and returns the process exit code.
pub fn run<G: Game>() -> i32 {
    let arguments: Vec<CString> = std::env::args()
        .map(|argument| CString::new(argument.replace('\0', "")).unwrap_or_default())
        .collect();

    let mut argv: Vec<*mut c_char> = arguments.iter().map(|argument| argument.as_ptr() as *mut c_char).collect();

    unsafe {
        if sys::Hyp_Initialize(argv.len() as c_int, argv.as_mut_ptr()) == 0 {
            return 1;
        }

        let game = create_game::<G>();

        if game.is_null() {
            sys::Hyp_Shutdown();

            return 1;
        }

        sys::Hyp_SetGame(game);

        if sys::Hyp_LaunchThreads() == 0 {
            return 1;
        }

        sys::Hyp_Shutdown();
    }

    0
}

/// Exports what the editor needs to load this crate (built as a cdylib) and play `$game`.
#[macro_export]
macro_rules! game_module {
    ($game:ty) => {
        #[no_mangle]
        pub extern "C" fn HypGameModule_CreateGame() -> *mut $crate::sys::Game {
            $crate::create_game::<$game>()
        }

        #[no_mangle]
        pub extern "C" fn HypGameModule_GetEngineVersion(major: *mut u32, minor: *mut u32, patch: *mut u32) {
            // bindings are resolved by name and checked by ABI version, so the module follows the engine it is loaded into
            unsafe { $crate::sys::Hyp_GetEngineVersion(major, minor, patch) }
        }
    };
}

/// The hash the engine's by-name lookups take (`World::get_scene_by_name`, `Node::find_child_by_name`).
pub fn string_hash(string: &str) -> StringHash {
    let string = object::to_c_string(string);
    let mut result = Name::default();

    // weak: computes the hash without registering the string as a name
    unsafe { sys::Name_FromString(string.as_ptr(), true, &mut result) };

    result.into()
}
