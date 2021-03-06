#!/usr/bin/env python3
def main():
    print("# Generated by vectors.py. Do not edit!")
    print(".globl alltraps")
    for i in range(256):
        print(".globl vector{0}".format(i))
        print("vector{0}:".format(i))
        # Push error code (0) not generated by the processor
        if not (i == 8 or (i >= 10 and i <= 14) or i == 17):
            print("    push $0")
        # Push trap number
        print("    push ${0}".format(i))
        print("    jmp alltraps")

    print("\n#vector table")
    print(".data")
    print(".globl vectors")
    print("vectors:")
    for i in range(256):
        print("    .quad vector{0}".format(i))

if __name__ == "__main__":
    main()
