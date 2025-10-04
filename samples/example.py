"""
This is a sample Python file created for editor testing purposes.

It contains a variety of Python syntax elements, including:
- Comments (single-line and multi-line)
- Variable declarations with different data types (int, float, string, list, dict, bool)
- Function definitions with docstrings
- Control flow statements (if/elif/else, for loops, while loops)
- Class and method definitions
- Error handling with try-except blocks
- A main execution block (`if __name__ == "__main__":`)

The purpose of this file is to provide a comprehensive, yet simple,
codebase for testing an editor's ability to correctly parse, highlight,
and interact with a Python script.
"""


text = "Hello, Moi!"
print(text)

# =================================================================
# Global Constants and Variables
# =================================================================

# A simple constant to be used later
PI = 3.141592653589793
EULER_NUMBER = 2.718281828459045

# A list of strings
FRUITS = ["apple", "banana", "cherry", "dragonfruit", "elderberry"]

# A dictionary mapping strings to integers
INVENTORY = {
    "apples": 50,
    "bananas": 100,
    "oranges": 75,
    "grapes": 1500,
}


# =================================================================
# Function Definitions
# =================================================================

def calculate_area(radius: float) -> float:
    """
    Calculates the area of a circle given its radius.

    Args:
        radius: The radius of the circle (a positive number).

    Returns:
        The calculated area.

    Raises:
        ValueError: If the radius is negative.
    """
    if radius < 0:
        # A specific error for a bad input value
        raise ValueError("Radius cannot be negative.")

    # Calculate the area using the PI constant
    area = PI * (radius ** 2)
    return area


def process_inventory(item_name: str, quantity_to_add: int) -> dict:
    """
    Adds a given quantity to an item in the global inventory.

    This function demonstrates conditional logic and dictionary manipulation.

    Args:
        item_name: The name of the item to update.
        quantity_to_add: The amount to add to the inventory.

    Returns:
        The updated inventory dictionary.
    """
    if item_name in INVENTORY:
        # Update the quantity if the item exists
        INVENTORY[item_name] += quantity_to_add
        print(f"Updated inventory for '{item_name}'. New total: {INVENTORY[item_name]}")
    else:
        # Add a new entry if the item does not exist
        INVENTORY[item_name] = quantity_to_add
        print(f"'{item_name}' not found. Adding with quantity: {quantity_to_add}")

    return INVENTORY


# =================================================================
# Class Definition
# =================================================================

class Person:
    """
    A simple class to represent a person.

    This class includes a constructor, a method, and a property.
    """

    def __init__(self, name: str, age: int):
        """
        Initializes a new Person instance.

        Args:
            name: The person's name.
            age: The person's age.
        """
        self.name = name
        self.age = age

    def get_info(self) -> str:
        """
        Returns a formatted string with the person's name and age.
        """
        return f"Name: {self.name}, Age: {self.age}"

    @property
    def is_adult(self) -> bool:
        """
        A property that checks if the person is an adult (age >= 18).
        """
        return self.age >= 18


# =================================================================
# Main Execution Block
# =================================================================

if __name__ == "__main__":
    print("--- Editor Test File Execution Start ---")

    # --- Section 1: Function Calls ---
    print("\n--- Section 1: Function Calls ---")
    try:
        # This call should work correctly
        radius_value = 5.0
        circle_area = calculate_area(radius_value)
        print(f"The area of a circle with radius {radius_value} is {circle_area:.2f}")

        # This call should raise an error
        # A simple comment on the line below
        calculate_area(-10)
    except ValueError as e:
        print(f"Caught an expected error: {e}")
    finally:
        print("Calculation block finished.")

    # --- Section 2: Loops and Conditionals ---
    print("\n--- Section 2: Loops and Conditionals ---")
    print("Iterating over the FRUITS list:")
    for fruit in FRUITS:
        if "e" in fruit:
            print(f"- Found a fruit with 'e': {fruit}")
        else:
            print(f"- {fruit} does not contain 'e'.")
    
    # A while loop example
    counter = 5
    print("\nCounting down with a while loop:")
    while counter > 0:
        print(f"Count: {counter}")
        counter -= 1
    
    # --- Section 3: Class Instantiation and Usage ---
    print("\n--- Section 3: Class Instantiation and Usage ---")
    # Instantiate a person object
    person1 = Person("Alice", 30)
    print(person1.get_info())

    # Use the @property
    if person1.is_adult:
        print(f"{person1.name} is an adult.")
    else:
        print(f"{person1.name} is not an adult.")
        
    # Another instance for comparison
    person2 = Person("Bob", 15)
    print(person2.get_info())
    if not person2.is_adult:
        print(f"{person2.name} is a minor.")

    # --- Section 4: Dictionary Manipulation ---
    print("\n--- Section 4: Dictionary Manipulation ---")
    print("Initial Inventory:")
    print(INVENTORY)
    
    # Call the processing function to update the inventory
    process_inventory("apples", 10)
    process_inventory("kiwis", 25)
    
    print("\nFinal Inventory:")
    print(INVENTORY)

    print("\n--- Editor Test File Execution End ---")