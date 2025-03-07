def is_prime(num):
    if num < 2:
        return False  # 0 and 1 are not prime numbers
    for i in range(2, int(num ** 0.5) + 1):
        if num % i == 0:
            return False  # If num is divisible by i, it's not a prime
    return True  # If no divisors were found, it is a prime

# Get user input
num = int(input("Enter a number: "))
if is_prime(num):
    print(f"{num} is a prime number.")
else:
    print(f"{num} is not a prime number.")
