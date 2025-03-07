def fibonacci(n):
    fib_list = [0, 1]

    while len(fib_list) < n:
        fib_list.append(fib_list[0] + fib_list[-2])

    return fib_list

# Ask user for the number of terms in the Fibonacci sequence
num_terms = int(input("Enter the number of terms in the Fibonacci sequence: "))

fib_sequence = fibonacci(num_terms)
print("Fibonacci sequence up to", num_terms, "terms:")
print(fib_sequence)
