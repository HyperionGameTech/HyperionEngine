# HypScript Language Reference

### Variables

Variables in HypScript are defined similarly to languages with optional typing, such as TypeScript. The type is placed after a colon proceding the name of the variable, in the style `x : Int`.

There are a few modifiers you can place _before_ the name of the variable to change its behavior. These modifiers are:

* `const` - A non-modifiable value
* `ref` - A reference to another value (may be combined with `const`)
* `let` - Applied by default, and completely optional. A modifiable value.

Variables may also be given the type `any`, to signify that they may be assigned to any type, with type checking performed dynamically.

### Functions

Functions are typically defined in this manner:

```
func MyFunction(x : Int, y : Int) : Int {
    return x + y;
}
```

Note that the ` : Int` after the function parameters signifies the return type of the function. If it is not provided, the return type will be deduced using the common type of all `return` statements in the function.

Functions may be stored as a variable, so you can change the function a variable holds.

Example:
```
do_thing : Function = func (x : Int) : Int {
    return x * 2;
};

// note that because we assigned the type as `Function`,
// which is a more broad-scope type than the actual type of the first function (`Function(Int) -> Int`),
// we can reassign to a function with different parameters, return type, etc.

do_thing = func () : String {
    return "Hello world";
};
```

Parameters that do not have an assigned type will be set to `any` automically.

You may also define function parameters as `ref`, so that when the function is called, those parameters will have their references directly passed in. Note that in order to pass in a value as a `ref` argument, is must be able to be modified. You can also define the parameter as `const ref`, so you can pass a `const` value in as well.

### Generics
Expressions in HypScript can be generic, similar to C++ templates. To create a value from a generic, pass in parameters using the `<Params, Here>` syntax.

Example:

```
const PI <T> : 3.141592;

pi_f : Float = PI<Float>;
pi_i : Int = PI<Int>;
```

Generic types:

```
class List<T> {
    ary: T[];

    func Push(self, item : T) {
        // push to self.ary ...
    }
}
```