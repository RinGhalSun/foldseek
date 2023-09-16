import os

input_dir = "/Users/zhengsun/pdb_data/vfn_v11/01_100_500_full"
output_dir = "./results_v11_full"

if not os.path.exists(output_dir):
    os.makedirs(output_dir)

pdb_files = [f for f in os.listdir(input_dir) if f.endswith(".pdb")]

for pdb_file in pdb_files:
    pdb_name, _ = os.path.splitext(pdb_file)
    aln_name_file = f"{pdb_name}-aln-name.txt"
    command = f"foldseek easy-search {os.path.join(input_dir, pdb_file)} pdb {os.path.join(output_dir, aln_name_file)} tmp --format-output query,target,alntmscore --alignment-type 1"
    
    os.system(command)

    print(f"Processed {pdb_file}")

print("All pdb files processed.")
